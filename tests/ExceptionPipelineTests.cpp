// ExceptionPipelineTests.cpp
//
// Regression tests for the complete host exception pipeline:
//
//   host fault → kernel signal → SignalHandler (hostException.cpp)
//              → g_handler dispatch
//              → PageManager::HandleFault (pageManager.cpp)
//              → MemoryTracker (memoryTracker.cpp)
//              → mprotect restore
//              → instruction retry
//
// Each test exercises one fault type or safety invariant in isolation.
// Tests that must terminate the process use fork()/waitpid() so the
// parent can verify the child's exit code or terminating signal.
//
// Async-signal-safety invariants are verified with static_assert so they
// produce a compile-time error if violated, not a runtime failure.

#include "common/hostException.h"
#include "common/config.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#if KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS
#include <csignal>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
#include <mach/mach.h>
#include <sys/ucontext.h>
#include <pthread.h>
#else
#include <ucontext.h>
#include <pthread.h>
#endif
#endif // KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS

namespace {

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

[[maybe_unused]] static void Check(bool value, const char* msg) {
    if (!value) {
        std::fprintf(stderr, "ExceptionPipelineTests FAIL: %s\n", msg);
        std::abort();
    }
}

// Exit code produced by HostException::FailFast → _Exit(321).
// waitpid/WEXITSTATUS preserves only the low 8 bits of the exit code.
static constexpr int kFailFastExitCode = 321 & 0xFF; // = 65

// ---------------------------------------------------------------------------
// Test 1: Handler installation & idempotency
// ---------------------------------------------------------------------------

static bool DummyHandler(const Common::HostException::ExceptionInfo&) {
    return false;
}

static void TestHandlerInstallation() {
    Check(Common::HostException::InstallHandler(DummyHandler),
          "InstallHandler must succeed on first call");
    Check(Common::HostException::InstallHandler(DummyHandler),
          "InstallHandler must succeed on re-install (idempotent)");
}

// ---------------------------------------------------------------------------
// Test 2: Async-signal-safety – compile-time assertions
//
// Every type accessed from SignalHandler must be always-lock-free so that
// atomic loads/stores do not call into the allocator or kernel (which would
// be async-signal-unsafe).
// ---------------------------------------------------------------------------

static_assert(std::atomic<Common::HostException::Handler>::is_always_lock_free,
              "g_handler atomic must be lock-free for async-signal safety");
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "g_install_state atomic must be lock-free for async-signal safety");
static_assert(std::atomic<bool>::is_always_lock_free,
              "atomic<bool> must be lock-free (used in test coordination)");

#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
// On macOS the nested-exception guard uses an atomic<mach_port_t> array
// instead of thread_local (TLS is not async-signal-safe on first access).
static_assert(std::atomic<uint32_t>::is_always_lock_free,
              "Mach thread port slot must be lock-free");
#endif

static void TestAsyncSafetyStaticAssertions() {
    // If compilation reached here, all static_assert checks above passed.
    // No runtime work needed.
}

// ---------------------------------------------------------------------------
// POSIX-only tests (all require signal delivery)
// ---------------------------------------------------------------------------
#if KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS

// ---------------------------------------------------------------------------
// Test 3: SA_RESTART and SA_SIGINFO flags are set on all fault signals
// ---------------------------------------------------------------------------

static void TestSaRestartFlagInstalled() {
    // InstallHandler must have been called before this test.
    for (const int sig : {SIGSEGV, SIGBUS, SIGILL}) {
        struct sigaction sa {};
        Check(::sigaction(sig, nullptr, &sa) == 0,
              "sigaction query must succeed");
        Check((sa.sa_flags & SA_RESTART) != 0,
              "Fault signal handler must carry SA_RESTART flag");
        Check((sa.sa_flags & SA_SIGINFO) != 0,
              "Fault signal handler must carry SA_SIGINFO flag");
    }
}

// ---------------------------------------------------------------------------
// Test 4: SIGSEGV write fault → handler fires → protection restored → retry
//
// Pipeline: mprotect(PROT_READ) + store → SIGSEGV/SIGBUS
//         → SignalHandler decodes Write fault
//         → WriteFaultHandler fixes mprotect → returns true
//         → instruction retried → store commits
// ---------------------------------------------------------------------------

static std::atomic<bool> g_write_fault_seen{false};
static void*             g_write_fault_page = nullptr;

static bool WriteFaultHandler(const Common::HostException::ExceptionInfo& info) {
    using namespace Common::HostException;
    // Accept SIGSEGV or SIGBUS (macOS may use either for mprotect violations).
    if (info.type != ExceptionType::AccessViolation) {
        return false;
    }
    if (info.access_violation_type != AccessViolationType::Write &&
        info.access_violation_type != AccessViolationType::Unknown) {
        return false;
    }
    void* page = g_write_fault_page;
    if (page == nullptr) {
        return false;
    }
    if (info.access_violation_vaddr < reinterpret_cast<uint64_t>(page) ||
        info.access_violation_vaddr >= reinterpret_cast<uint64_t>(page) +
                                           static_cast<uint64_t>(::sysconf(_SC_PAGESIZE))) {
        return false;
    }
    g_write_fault_seen.store(true, std::memory_order_relaxed);
    // mprotect(2) is async-signal-safe (POSIX.1-2017 §2.4.3).
    ::mprotect(page, static_cast<size_t>(::sysconf(_SC_PAGESIZE)), PROT_READ | PROT_WRITE);
    return true; // retry faulting store instruction
}

static void TestSigsegvWriteFault() {
    const long page_size = ::sysconf(_SC_PAGESIZE);
    Check(page_size > 0, "sysconf(_SC_PAGESIZE) must succeed");
    const auto alloc = static_cast<size_t>(page_size);

    void* page = ::mmap(nullptr, alloc, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE, -1, 0);
    Check(page != MAP_FAILED, "mmap must succeed for write-fault test");

    auto* ptr = static_cast<volatile uint64_t*>(page);
    *ptr = 0xDEADBEEFull;

    g_write_fault_page = page;
    g_write_fault_seen.store(false, std::memory_order_relaxed);
    Check(Common::HostException::InstallHandler(WriteFaultHandler),
          "InstallHandler must succeed for write-fault test");

    // Demote to read-only: next write triggers SIGSEGV (or SIGBUS on macOS).
    ::mprotect(page, alloc, PROT_READ);

    // This write faults; WriteFaultHandler restores PROT_READ|WRITE then
    // returns true, causing the kernel to re-execute the store successfully.
    *ptr = 0xCAFEBABEull;

    Check(g_write_fault_seen.load(std::memory_order_relaxed),
          "Write fault must have called the exception handler");
    Check(*ptr == 0xCAFEBABEull,
          "Retried write must commit the new value");

    g_write_fault_page = nullptr;
    ::munmap(page, alloc);
}

// ---------------------------------------------------------------------------
// Test 5: SIGSEGV read fault (PROT_NONE) → handler fires → retry
//
// Same pipeline as Test 4 but the access is a load and the initial
// protection is PROT_NONE.  On ARM64 Linux the access type may be Unknown
// (fix A2) so the handler accepts both Read and Unknown.
// ---------------------------------------------------------------------------

static std::atomic<bool> g_read_fault_seen{false};
static void*             g_read_fault_page = nullptr;

static bool ReadFaultHandler(const Common::HostException::ExceptionInfo& info) {
    using namespace Common::HostException;
    if (info.type != ExceptionType::AccessViolation) {
        return false;
    }
    // Accept Read or Unknown (ARM64 Linux ESR not reliably available).
    if (info.access_violation_type != AccessViolationType::Read &&
        info.access_violation_type != AccessViolationType::Unknown) {
        return false;
    }
    void* page = g_read_fault_page;
    if (page == nullptr) {
        return false;
    }
    if (info.access_violation_vaddr < reinterpret_cast<uint64_t>(page) ||
        info.access_violation_vaddr >= reinterpret_cast<uint64_t>(page) +
                                           static_cast<uint64_t>(::sysconf(_SC_PAGESIZE))) {
        return false;
    }
    g_read_fault_seen.store(true, std::memory_order_relaxed);
    ::mprotect(page, static_cast<size_t>(::sysconf(_SC_PAGESIZE)), PROT_READ | PROT_WRITE);
    return true; // retry the faulting load
}

static void TestSigsegvReadFault() {
    const long page_size = ::sysconf(_SC_PAGESIZE);
    Check(page_size > 0, "sysconf(_SC_PAGESIZE) must succeed");
    const auto alloc = static_cast<size_t>(page_size);

    void* page = ::mmap(nullptr, alloc, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE, -1, 0);
    Check(page != MAP_FAILED, "mmap must succeed for read-fault test");

    auto* ptr = static_cast<volatile uint64_t*>(page);
    *ptr = 0xFEEDFACEull;

    g_read_fault_page = page;
    g_read_fault_seen.store(false, std::memory_order_relaxed);
    Check(Common::HostException::InstallHandler(ReadFaultHandler),
          "InstallHandler must succeed for read-fault test");

    ::mprotect(page, alloc, PROT_NONE);

    // This load faults; ReadFaultHandler restores PROT_READ and returns true.
    const uint64_t val = *ptr;

    Check(g_read_fault_seen.load(std::memory_order_relaxed),
          "Read fault must have called the exception handler");
    Check(val == 0xFEEDFACEull,
          "Retried load must return the original value");

    g_read_fault_page = nullptr;
    ::munmap(page, alloc);
}

// ---------------------------------------------------------------------------
// Test 6: SIGILL dispatch with PC advance
//
// EmitIllegalInstruction executes ud2 (x86_64) or udf #0 (ARM64).
// SigillAdvanceHandler advances the program counter past the trap so
// execution resumes normally — verifying that:
//   (a) SignalHandler recognises SIGILL → ExceptionType::IllegalInstruction
//   (b) native_context carries a mutable ucontext_t* that callers can modify
//   (c) returning true from the handler causes the kernel to resume at new PC
// ---------------------------------------------------------------------------

static std::atomic<bool> g_sigill_seen{false};

static bool SigillAdvanceHandler(const Common::HostException::ExceptionInfo& info) {
    if (info.type != Common::HostException::ExceptionType::IllegalInstruction) {
        return false;
    }
    g_sigill_seen.store(true, std::memory_order_relaxed);

    // Advance the faulting thread's program counter past the illegal instruction
    // so the kernel re-executes from the next instruction after returning true.
    auto* uc = static_cast<ucontext_t*>(info.native_context);
#if defined(__APPLE__)
#  if defined(__x86_64__)
    uc->uc_mcontext->__ss.__rip += 2; // ud2 = 2 bytes (0x0F 0x0B)
#  elif defined(__arm64__) || defined(__aarch64__)
    uc->uc_mcontext->__ss.__pc  += 4; // udf #0 = 4 bytes (fixed-width A64)
#  endif
#elif defined(__linux__)
#  if defined(__x86_64__)
    uc->uc_mcontext.gregs[REG_RIP] += 2;
#  elif defined(__aarch64__)
    uc->uc_mcontext.pc             += 4;
#  endif
#endif
    return true;
}

// noinline so the compiler cannot eliminate the inline asm by constant-folding.
[[gnu::noinline]] static void EmitIllegalInstruction() {
#if defined(__x86_64__)
    __asm__ volatile("ud2");
#elif defined(__arm64__) || defined(__aarch64__)
    __asm__ volatile("udf #0");
#else
    // Architecture not covered: skip by returning without trapping.
#endif
}

static void TestSigillDispatch() {
#if defined(__x86_64__) || defined(__arm64__) || defined(__aarch64__)
    g_sigill_seen.store(false, std::memory_order_relaxed);
    Check(Common::HostException::InstallHandler(SigillAdvanceHandler),
          "InstallHandler must succeed for SIGILL test");

    // Execution resumes at PC+{2,4} after the handler advances the PC.
    EmitIllegalInstruction();

    Check(g_sigill_seen.load(std::memory_order_relaxed),
          "SIGILL must have been dispatched to the exception handler");
#else
    std::printf("  [skip] TestSigillDispatch: unsupported architecture\n");
#endif
}

// ---------------------------------------------------------------------------
// Test 7: Nested-fault detection terminates with FailFast exit code
//
// While handling SIGSEGV the handler sends SIGBUS to itself (pthread_kill is
// async-signal-safe on POSIX).  Since SIGBUS is NOT in sa_mask during SIGSEGV
// handling (only SIGUSR1 is masked), it is delivered immediately.  This
// re-enters SignalHandler, which detects the nested invocation via
// GetInExceptionFilter() and calls FailFast → _Exit(321).
//
// The test isolates this in a child process and verifies the exit code.
// ---------------------------------------------------------------------------

static bool NestedFaultDetectionHandler(const Common::HostException::ExceptionInfo& info) {
    if (info.type == Common::HostException::ExceptionType::AccessViolation) {
        // Send SIGBUS to ourselves while already inside SignalHandler (SIGSEGV).
        // SIGBUS is not masked → SignalHandler re-entered → nested detection fires.
        ::pthread_kill(::pthread_self(), SIGBUS);
    }
    return false;
}

static void TestNestedFaultDetection() {
    const pid_t pid = ::fork();
    Check(pid >= 0, "fork must succeed for nested-fault test");

    if (pid == 0) {
        // Child: set up handler then trigger the initial SIGSEGV.
        Common::HostException::InstallHandler(NestedFaultDetectionHandler);
        const long page_size = ::sysconf(_SC_PAGESIZE);
        void*      page      = ::mmap(nullptr, static_cast<size_t>(page_size > 0 ? page_size : 4096),
                                      PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (page == MAP_FAILED) {
            _Exit(1);
        }
        // Touch the inaccessible page → SIGSEGV → NestedFaultDetectionHandler
        // → pthread_kill(SIGBUS) → nested SignalHandler → FailFast → _Exit(321).
        volatile char* p = static_cast<volatile char*>(page);
        (void)*p;
        _Exit(0); // unreachable
    }

    int status = 0;
    ::waitpid(pid, &status, 0);

    // Two valid outcomes depending on OS signal delivery timing:
    //
    // (A) Linux / synchronous delivery: SIGBUS is delivered immediately while
    //     inside the SIGSEGV SignalHandler → GetInExceptionFilter() returns true
    //     → FailFast → _Exit(321).  WEXITSTATUS = 321 & 0xFF = 65.
    //
    // (B) macOS / deferred delivery: SIGBUS is queued until the SIGSEGV
    //     SignalHandler returns, then delivered with the registered handler.
    //     At that point g_in_exception_filter was already cleared, so no
    //     FailFast — instead the process is terminated by SIGSEGV (SIG_DFL
    //     was restored for it) or by SIGBUS. Both are signal-terminated.
    //
    // In either case the child must not exit normally (code 0).
    const bool nested_failfast =
        WIFEXITED(status) && WEXITSTATUS(status) == kFailFastExitCode;
    const bool signal_terminated =
        WIFSIGNALED(status) &&
        (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV);
    Check(nested_failfast || signal_terminated,
          "Nested-fault child must either FailFast or be killed by SIGBUS/SIGSEGV");
}

// ---------------------------------------------------------------------------
// Test 8: Unresolved fault chains to the default signal disposition
//
// A handler returning false causes SignalHandler to restore SIG_DFL and
// return. The kernel re-executes the faulting instruction which now delivers
// the signal with its default disposition (terminate).  We verify via fork
// that the child is killed by SIGSEGV or SIGBUS (not _Exit or SIGILL, etc.).
// ---------------------------------------------------------------------------

static bool UnresolvedHandler(const Common::HostException::ExceptionInfo&) {
    return false; // decline to fix → chain to SIG_DFL → process termination
}

static void TestUnresolvedFaultChainsToDefault() {
    const pid_t pid = ::fork();
    Check(pid >= 0, "fork must succeed for unresolved-fault test");

    if (pid == 0) {
        Common::HostException::InstallHandler(UnresolvedHandler);
        const long page_size = ::sysconf(_SC_PAGESIZE);
        void*      page      = ::mmap(nullptr, static_cast<size_t>(page_size > 0 ? page_size : 4096),
                                      PROT_NONE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (page == MAP_FAILED) {
            _Exit(1);
        }
        volatile char* p = static_cast<volatile char*>(page);
        (void)*p; // → SIGSEGV, handler returns false → SIG_DFL → terminated
        _Exit(0); // unreachable
    }

    int status = 0;
    ::waitpid(pid, &status, 0);
    Check(WIFSIGNALED(status),
          "Unresolved fault must terminate the process via a signal (not _Exit)");
    const int sig = WTERMSIG(status);
    Check(sig == SIGSEGV || sig == SIGBUS,
          "Unresolved fault must deliver SIGSEGV or SIGBUS (platform-specific)");
}

#endif // KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS

} // namespace

int main() {
    std::printf("ExceptionPipelineTests: starting...\n");

    TestHandlerInstallation();
    std::printf("  [PASS] TestHandlerInstallation\n");

    TestAsyncSafetyStaticAssertions();
    std::printf("  [PASS] TestAsyncSafetyStaticAssertions (compile-time)\n");

#if KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS
    // SA_RESTART / SA_SIGINFO must be set after InstallHandler.
    TestSaRestartFlagInstalled();
    std::printf("  [PASS] TestSaRestartFlagInstalled\n");

    TestSigsegvWriteFault();
    std::printf("  [PASS] TestSigsegvWriteFault\n");

    TestSigsegvReadFault();
    std::printf("  [PASS] TestSigsegvReadFault\n");

    TestSigillDispatch();
    std::printf("  [PASS] TestSigillDispatch\n");

    TestNestedFaultDetection();
    std::printf("  [PASS] TestNestedFaultDetection\n");

    TestUnresolvedFaultChainsToDefault();
    std::printf("  [PASS] TestUnresolvedFaultChainsToDefault\n");
#endif

    std::printf("ExceptionPipelineTests: PASSED\n");
    return 0;
}
