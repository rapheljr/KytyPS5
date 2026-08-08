#include "common/hostException.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
#include <windows.h> // IWYU pragma: keep
#elif defined(__APPLE__)
#include <csignal>
#include <mach/mach.h>
#include <pthread.h>
#include <shared_mutex>
#include <sys/ucontext.h>
#include <unistd.h>
#include <unordered_map>
#else
#include <csignal>
#include <initializer_list>
#include <ucontext.h> // IWYU pragma: keep
#include <unistd.h>
#endif

// IWYU pragma: no_include <errhandlingapi.h>
// IWYU pragma: no_include <excpt.h>
// IWYU pragma: no_include <minwinbase.h>
// IWYU pragma: no_include <minwindef.h>
// IWYU pragma: no_include <wtypes.h>

namespace Common::HostException {

#if !defined(__APPLE__)

static std::atomic<Handler> g_handler {nullptr};
static std::atomic_uint32_t g_install_state {0};
static thread_local bool    g_in_exception_filter = false;

static_assert(decltype(g_handler)::is_always_lock_free);
static_assert(decltype(g_install_state)::is_always_lock_free);

[[noreturn]] static void FailFast(const char* reason) noexcept {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	// Windows VEH runs in normal thread context — FILE* I/O is safe here.
	std::fputs("HostException fail-fast: ", stderr);
	std::fputs(reason != nullptr ? reason : "unspecified", stderr);
	std::fputc('\n', stderr);
	std::fflush(stderr);
	TerminateProcess(GetCurrentProcess(), static_cast<UINT>(EXCEPTION_NONCONTINUABLE_EXCEPTION));
#else
	// POSIX: use write(2) which is async-signal-safe (POSIX.1-2017 §2.4.3).
	// std::fputs / std::fflush hold the FILE lock and are NOT async-signal-safe.
	static constexpr char kPrefix[] = "HostException fail-fast: ";
	static constexpr char kUnspec[] = "unspecified";
	static constexpr char kNewline  = '\n';
	::write(STDERR_FILENO, kPrefix, sizeof(kPrefix) - 1);
	if (reason != nullptr) {
		::write(STDERR_FILENO, reason, std::strlen(reason));
	} else {
		::write(STDERR_FILENO, kUnspec, sizeof(kUnspec) - 1);
	}
	::write(STDERR_FILENO, &kNewline, 1);
#endif
	std::_Exit(321);
}

class FilterScope final {
public:
	FilterScope() noexcept {
		if (g_in_exception_filter) {
			FailFast("nested exception while resolving a host fault");
		}
		g_in_exception_filter = true;
	}

	~FilterScope() { g_in_exception_filter = false; }

	KYTY_CLASS_NO_COPY(FilterScope);
};

static Handler LoadInstalledHandler() noexcept {
	if (g_install_state.load(std::memory_order_acquire) == 0) {
		FailFast("host exception handler is not installed");
	}

	const auto handler = g_handler.load(std::memory_order_acquire);
	if (handler == nullptr) {
		FailFast("host exception callback is null");
	}
	return handler;
}
#endif

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS

static LONG WINAPI ExceptionFilter(PEXCEPTION_POINTERS exception) {
	FilterScope filter_scope;

	auto* exception_record = exception->ExceptionRecord;

	if (exception_record->ExceptionCode == DBG_PRINTEXCEPTION_C ||
	    exception_record->ExceptionCode == DBG_PRINTEXCEPTION_WIDE_C) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (exception_record->ExceptionCode == 0x406D1388) {
		// Set a thread name.
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	ExceptionInfo info {};
	info.exception_address = reinterpret_cast<uint64_t>(exception_record->ExceptionAddress);
	info.native_code       = exception_record->ExceptionCode;
	info.native_context    = exception->ContextRecord;

	if (exception_record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) {
		info.type = ExceptionType::AccessViolation;
		switch (exception_record->ExceptionInformation[0]) {
			case 0: info.access_violation_type = AccessViolationType::Read; break;
			case 1: info.access_violation_type = AccessViolationType::Write; break;
			case 8: info.access_violation_type = AccessViolationType::Execute; break;
			default: info.access_violation_type = AccessViolationType::Unknown; break;
		}
		info.access_violation_vaddr = exception_record->ExceptionInformation[1];
	} else if (exception_record->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		printf("Unhandled win exception: code=0x%08" PRIx32 ", addr=0x%016" PRIx64
		       ", rip=0x%016" PRIx64 ", rsp=0x%016" PRIx64 ", rbp=0x%016" PRIx64 "\n",
		       static_cast<uint32_t>(exception_record->ExceptionCode),
		       reinterpret_cast<uint64_t>(exception_record->ExceptionAddress),
		       exception->ContextRecord->Rip, exception->ContextRecord->Rsp,
		       exception->ContextRecord->Rbp);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	info.rax = exception->ContextRecord->Rax;
	info.rbx = exception->ContextRecord->Rbx;
	info.rcx = exception->ContextRecord->Rcx;
	info.rdx = exception->ContextRecord->Rdx;
	info.rsi = exception->ContextRecord->Rsi;
	info.rdi = exception->ContextRecord->Rdi;
	info.rbp = exception->ContextRecord->Rbp;
	info.rsp = exception->ContextRecord->Rsp;
	info.r8  = exception->ContextRecord->R8;
	info.r9  = exception->ContextRecord->R9;
	info.r10 = exception->ContextRecord->R10;
	info.r11 = exception->ContextRecord->R11;
	info.r12 = exception->ContextRecord->R12;
	info.r13 = exception->ContextRecord->R13;
	info.r14 = exception->ContextRecord->R14;
	info.r15 = exception->ContextRecord->R15;

	const auto handler = LoadInstalledHandler();

	return handler(info) ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_CONTINUE_SEARCH;
}

#elif defined(__APPLE__)

static std::atomic<Handler> g_handler {nullptr};
static std::atomic_uint32_t g_install_state {0};
namespace {
constexpr size_t MAX_FILTER_THREADS = 64;
std::atomic<mach_port_t> g_filter_threads[MAX_FILTER_THREADS] {};

bool GetInExceptionFilter() noexcept {
	const mach_port_t self = pthread_mach_thread_np(pthread_self());
	for (size_t i = 0; i < MAX_FILTER_THREADS; i++) {
		if (g_filter_threads[i].load(std::memory_order_relaxed) == self) {
			return true;
		}
	}
	return false;
}

void SetInExceptionFilter(bool value) noexcept {
	const mach_port_t self = pthread_mach_thread_np(pthread_self());
	if (value) {
		for (size_t i = 0; i < MAX_FILTER_THREADS; i++) {
			mach_port_t expected = 0;
			if (g_filter_threads[i].compare_exchange_strong(expected, self, std::memory_order_relaxed)) {
				return;
			}
		}
	} else {
		for (size_t i = 0; i < MAX_FILTER_THREADS; i++) {
			if (g_filter_threads[i].load(std::memory_order_relaxed) == self) {
				g_filter_threads[i].store(0, std::memory_order_relaxed);
				return;
			}
		}
	}
}
} // namespace

static_assert(decltype(g_handler)::is_always_lock_free);
static_assert(decltype(g_install_state)::is_always_lock_free);

[[noreturn]] static void FailFast(const char* reason) noexcept {
	// Use write(2): async-signal-safe per POSIX.1-2017 §2.4.3.
	// std::fputs / std::fflush hold the FILE lock and are NOT async-signal-safe.
	static constexpr char kPrefix[] = "HostException fail-fast: ";
	static constexpr char kUnspec[] = "unspecified";
	static constexpr char kNewline  = '\n';
	::write(STDERR_FILENO, kPrefix, sizeof(kPrefix) - 1);
	if (reason != nullptr) {
		::write(STDERR_FILENO, reason, std::strlen(reason));
	} else {
		::write(STDERR_FILENO, kUnspec, sizeof(kUnspec) - 1);
	}
	::write(STDERR_FILENO, &kNewline, 1);
	std::_Exit(321);
}

// Translate the x86-64 page-fault error code (mcontext __es.__err) into an access type.
// bit 1 (0x2) = write, bit 4 (0x10) = instruction fetch, otherwise a read.
static AccessViolationType DecodeAccess(uint64_t err) {
	if ((err & 0x10u) != 0) {
		return AccessViolationType::Execute;
	}
	if ((err & 0x2u) != 0) {
		return AccessViolationType::Write;
	}
	return AccessViolationType::Read;
}

// POSIX signal handler that mirrors the Windows vectored handler: build an ExceptionInfo
// from the mcontext and dispatch. A resolved fault (handler returns true) simply returns,
// re-executing the faulting instruction against the now-fixed protection. An unresolved
// fault restores the default disposition so the retry terminates the process.
static void SignalHandler(int sig, siginfo_t* si, void* uctx) {
#if defined(__APPLE__)
	if (GetInExceptionFilter()) {
		FailFast("nested exception while resolving a host fault");
	}
	SetInExceptionFilter(true);
#else
	if (g_in_exception_filter) {
		FailFast("nested exception while resolving a host fault");
	}
	g_in_exception_filter = true;
#endif

	auto*       uc = static_cast<ucontext_t*>(uctx);
	ExceptionInfo info {};
	info.native_code    = static_cast<uint32_t>(si->si_code);
	info.native_context = uctx;

#if defined(__APPLE__)
	const auto* mc = uc->uc_mcontext;
#if defined(__arm64__) || defined(__aarch64__)
	const auto& ss = mc->__ss;
	info.exception_address = ss.__pc;
	if (sig == SIGILL) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		info.type                   = ExceptionType::AccessViolation;
		const uint64_t esr          = mc->__es.__esr;
		const bool     is_write     = (esr & 0x40u) != 0;
		info.access_violation_type  = is_write ? AccessViolationType::Write : AccessViolationType::Read;
		info.access_violation_vaddr = reinterpret_cast<uint64_t>(si->si_addr);
	}

	info.rax = ss.__x[0];
	info.rbx = ss.__x[1];
	info.rcx = ss.__x[2];
	info.rdx = ss.__x[3];
	info.rsi = ss.__x[4];
	info.rdi = ss.__x[5];
	info.rbp = ss.__fp;
	info.rsp = ss.__sp;
	info.r8  = ss.__x[8];
	info.r9  = ss.__x[9];
	info.r10 = ss.__x[10];
	info.r11 = ss.__x[11];
	info.r12 = ss.__x[12];
	info.r13 = ss.__x[13];
	info.r14 = ss.__x[14];
	info.r15 = ss.__x[15];
#else
	const auto& ss = mc->__ss;
	info.exception_address = ss.__rip;
	if (sig == SIGILL) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		info.type                   = ExceptionType::AccessViolation;
		info.access_violation_type  = DecodeAccess(mc->__es.__err);
		info.access_violation_vaddr = reinterpret_cast<uint64_t>(si->si_addr);
	}

	info.rax = ss.__rax;
	info.rbx = ss.__rbx;
	info.rcx = ss.__rcx;
	info.rdx = ss.__rdx;
	info.rsi = ss.__rsi;
	info.rdi = ss.__rdi;
	info.rbp = ss.__rbp;
	info.rsp = ss.__rsp;
	info.r8  = ss.__r8;
	info.r9  = ss.__r9;
	info.r10 = ss.__r10;
	info.r11 = ss.__r11;
	info.r12 = ss.__r12;
	info.r13 = ss.__r13;
	info.r14 = ss.__r14;
	info.r15 = ss.__r15;
#endif
#else
	const auto* mc = &uc->uc_mcontext;
#if defined(__aarch64__)
	info.exception_address = mc->pc;
	if (sig == SIGILL) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		info.type = ExceptionType::AccessViolation;
		// Linux ARM64: the ESR (Exception Syndrome Register) is not exposed through
		// the standard ucontext_t fields on all kernel versions, so we cannot reliably
		// distinguish read from write faults here. Report Unknown rather than silently
		// misidentifying every read fault as a write. Callers that need the distinction
		// can inspect their own page-watcher state (write_watchers vs access_watchers).
		info.access_violation_type  = AccessViolationType::Unknown;
		info.access_violation_vaddr = reinterpret_cast<uint64_t>(si->si_addr);
	}

	info.rax = mc->regs[0];
	info.rbx = mc->regs[1];
	info.rcx = mc->regs[2];
	info.rdx = mc->regs[3];
	info.rsi = mc->regs[4];
	info.rdi = mc->regs[5];
	info.rbp = mc->regs[29];
	info.rsp = mc->sp;
	info.r8  = mc->regs[8];
	info.r9  = mc->regs[9];
	info.r10 = mc->regs[10];
	info.r11 = mc->regs[11];
	info.r12 = mc->regs[12];
	info.r13 = mc->regs[13];
	info.r14 = mc->regs[14];
	info.r15 = mc->regs[15];
#else
	const auto* gregs = mc->gregs;
	info.exception_address = static_cast<uint64_t>(gregs[REG_RIP]);
	if (sig == SIGILL) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		info.type                   = ExceptionType::AccessViolation;
		info.access_violation_type  = DecodeAccess(static_cast<uint64_t>(gregs[REG_ERR]));
		info.access_violation_vaddr = reinterpret_cast<uint64_t>(si->si_addr);
	}

	info.rax = static_cast<uint64_t>(gregs[REG_RAX]);
	info.rbx = static_cast<uint64_t>(gregs[REG_RBX]);
	info.rcx = static_cast<uint64_t>(gregs[REG_RCX]);
	info.rdx = static_cast<uint64_t>(gregs[REG_RDX]);
	info.rsi = static_cast<uint64_t>(gregs[REG_RSI]);
	info.rdi = static_cast<uint64_t>(gregs[REG_RDI]);
	info.rbp = static_cast<uint64_t>(gregs[REG_RBP]);
	info.rsp = static_cast<uint64_t>(gregs[REG_RSP]);
	info.r8  = static_cast<uint64_t>(gregs[REG_R8]);
	info.r9  = static_cast<uint64_t>(gregs[REG_R9]);
	info.r10 = static_cast<uint64_t>(gregs[REG_R10]);
	info.r11 = static_cast<uint64_t>(gregs[REG_R11]);
	info.r12 = static_cast<uint64_t>(gregs[REG_R12]);
	info.r13 = static_cast<uint64_t>(gregs[REG_R13]);
	info.r14 = static_cast<uint64_t>(gregs[REG_R14]);
	info.r15 = static_cast<uint64_t>(gregs[REG_R15]);
#endif
#endif

	const auto handler = g_handler.load(std::memory_order_acquire);
	if (handler == nullptr) {
		FailFast("host exception callback is null");
	}

	const bool resolved = handler(info);
#if defined(__APPLE__)
	SetInExceptionFilter(false);
#else
	g_in_exception_filter = false;
#endif

	if (resolved) {
		return; // retry the faulting instruction against the fixed mapping
	}

	// Unresolved: restore the default action so the re-executed instruction terminates.
	struct sigaction dfl {};
	dfl.sa_handler = SIG_DFL;
	sigemptyset(&dfl.sa_mask);
	sigaction(sig, &dfl, nullptr);
}

#else

// x86-64 page-fault error bits.
constexpr uint64_t PAGE_FAULT_ERROR_WRITE       = 0x02;
constexpr uint64_t PAGE_FAULT_ERROR_INSTRUCTION = 0x10;

// Let the kernel handle an unresolved fault on retry.
static void ChainToDefault(int signal_number) noexcept {
	struct sigaction restore {};
	restore.sa_handler = SIG_DFL;
	sigemptyset(&restore.sa_mask);
	restore.sa_flags = 0;
	::sigaction(signal_number, &restore, nullptr);
}

static void SignalHandler(int signal_number, siginfo_t* signal_info, void* native_context) {
	FilterScope filter_scope;

	auto* context = static_cast<ucontext_t*>(native_context);

	ExceptionInfo info {};
	info.native_code    = static_cast<uint32_t>(signal_number);
	info.native_context = context;

#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
	auto* mc = context->uc_mcontext;
	info.exception_address = mc->__ss.__pc;
	info.rsp = mc->__ss.__sp;
	info.rbp = mc->__ss.__fp;
	info.rax = mc->__ss.__x[0];
	info.rbx = mc->__ss.__x[19];
	info.rcx = mc->__ss.__x[1];
	info.rdx = mc->__ss.__x[2];
	info.rsi = mc->__ss.__x[3];
	info.rdi = mc->__ss.__x[4];
	info.r8  = mc->__ss.__x[5];
	info.r9  = mc->__ss.__x[6];
	info.r10 = mc->__ss.__x[7];
	info.r11 = mc->__ss.__x[8];
	info.r12 = mc->__ss.__x[20];
	info.r13 = mc->__ss.__x[21];
	info.r14 = mc->__ss.__x[22];
	info.r15 = mc->__ss.__x[23];
#else
	auto* gregs = context->uc_mcontext.gregs;
	info.exception_address = static_cast<uint64_t>(gregs[REG_RIP]);
	info.rax = static_cast<uint64_t>(gregs[REG_RAX]);
	info.rbx = static_cast<uint64_t>(gregs[REG_RBX]);
	info.rcx = static_cast<uint64_t>(gregs[REG_RCX]);
	info.rdx = static_cast<uint64_t>(gregs[REG_RDX]);
	info.rsi = static_cast<uint64_t>(gregs[REG_RSI]);
	info.rdi = static_cast<uint64_t>(gregs[REG_RDI]);
	info.rbp = static_cast<uint64_t>(gregs[REG_RBP]);
	info.rsp = static_cast<uint64_t>(gregs[REG_RSP]);
	info.r8  = static_cast<uint64_t>(gregs[REG_R8]);
	info.r9  = static_cast<uint64_t>(gregs[REG_R9]);
	info.r10 = static_cast<uint64_t>(gregs[REG_R10]);
	info.r11 = static_cast<uint64_t>(gregs[REG_R11]);
	info.r12 = static_cast<uint64_t>(gregs[REG_R12]);
	info.r13 = static_cast<uint64_t>(gregs[REG_R13]);
	info.r14 = static_cast<uint64_t>(gregs[REG_R14]);
	info.r15 = static_cast<uint64_t>(gregs[REG_R15]);
#endif

	if (signal_number == SIGSEGV || signal_number == SIGBUS) {
		info.type = ExceptionType::AccessViolation;
		info.access_violation_type = AccessViolationType::Read;
		info.access_violation_vaddr = reinterpret_cast<uint64_t>(signal_info->si_addr);
	} else if (signal_number == SIGILL) {
		info.type = ExceptionType::IllegalInstruction;
	} else {
		ChainToDefault(signal_number);
		return;
	}

	const auto handler = LoadInstalledHandler();

	if (handler(info)) {
		return;
	}

	ChainToDefault(signal_number);
}

#endif

bool InstallHandler(Handler handler) {
	if (handler == nullptr) {
		return false;
	}

	uint32_t expected_state = 0;
	if (!g_install_state.compare_exchange_strong(expected_state, 1, std::memory_order_acq_rel)) {
		if (g_install_state.load(std::memory_order_acquire) == 2) {
			g_handler.store(handler, std::memory_order_release);
			return true;
		}
		return false;
	}

	g_handler.store(handler, std::memory_order_release);

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	if (AddVectoredExceptionHandler(1, ExceptionFilter) == nullptr) {
		g_handler.store(nullptr, std::memory_order_release);
		g_install_state.store(0, std::memory_order_release);
		printf("AddVectoredExceptionHandler() failed\n");
		return false;
	}
#elif defined(__APPLE__)
	struct sigaction sa {};
	sa.sa_sigaction = SignalHandler;
	sa.sa_flags     = SA_SIGINFO | SA_RESTART;
	sigemptyset(&sa.sa_mask);
	// The guest signal-dispatch path (KernelRaiseException) interrupts threads with
	// SIGUSR1; block it while a fault is being resolved so a stop-the-world request
	// cannot preempt the handler between the protection fix and the retry.
	sigaddset(&sa.sa_mask, SIGUSR1);

	// macOS raises SIGBUS for protection faults on some paths and SIGSEGV on others;
	// SIGILL covers instructions the host cannot execute (routed to the x64 emulator).
	bool ok = sigaction(SIGSEGV, &sa, nullptr) == 0 && sigaction(SIGBUS, &sa, nullptr) == 0 &&
	          sigaction(SIGILL, &sa, nullptr) == 0;
	if (!ok) {
		g_handler.store(nullptr, std::memory_order_release);
		g_install_state.store(0, std::memory_order_release);
		printf("sigaction() failed to install the host fault handler\n");
		return false;
	}
#else
	struct sigaction action {};
	action.sa_sigaction = SignalHandler;
	sigemptyset(&action.sa_mask);
	sigaddset(&action.sa_mask, SIGRTMIN + 3);
	sigaddset(&action.sa_mask, SIGUSR1);
	// Fault resolution needs the normal thread stack.
	action.sa_flags = SA_SIGINFO | SA_RESTART;

	for (const int signal_number: {SIGSEGV, SIGBUS, SIGILL}) {
		if (::sigaction(signal_number, &action, nullptr) != 0) {
			g_handler.store(nullptr, std::memory_order_release);
			g_install_state.store(0, std::memory_order_release);
			printf("sigaction(%d) failed\n", signal_number);
			return false;
		}
	}
#endif

	g_install_state.store(2, std::memory_order_release);
	return true;
}

} // namespace Common::HostException
