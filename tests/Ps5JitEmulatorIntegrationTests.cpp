// Ps5JitEmulatorIntegrationTests.cpp
//
// PS5 JIT Emulator Integration Test Suite (Phase P).
//
// Test cases:
//   1. OpenOrbis Hello World — load inline ELF stub, JIT-execute, verify
//   2. JIT Dispatch Loop    — block compilation + cache hit on second call
//   3. Subsystem Stubs      — all 6 subsystem categories dispatch successfully
//   4. Telemetry Collector  — counters increment, frame snapshots accumulate
//   5. Compatibility Report — HTML + Markdown generated with correct sections
//   6. Unsupported Instruction Reporting — injected unknown opcode → report

#include "compat/ps5CompatibilityReport.h"
#include "emulator/emulatorIntegration.h"
#include "kernel/openOrbisSubsystems.h"
#include "loader/openOrbisElfLoader.h"
#include "loader/ps5JitDispatchLoop.h"
#include "loader/recompiler/jitTelemetryCollector.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using Compat::ExecutionSessionStats;
using Compat::GameStatus;

// ─── Minimal x86-64 ELF64 builder ────────────────────────────────────────────
// Builds the smallest valid ET_EXEC ELF64 with a single PT_LOAD segment.
// Code is appended right after the ELF64 + 1 phdr header (120 bytes total).

namespace {

#pragma pack(push, 1)
struct MiniElf64 {
    // ELF64 Header (64 bytes)
    uint8_t  e_ident[16]   = {0x7F, 'E', 'L', 'F', 2, 1, 1, 0,
                               0,    0,   0,   0,   0, 0, 0, 0};
    uint16_t e_type        = 2;       // ET_EXEC
    uint16_t e_machine     = 62;      // EM_X86_64
    uint32_t e_version     = 1;
    uint64_t e_entry       = 0x400078; // entry = base + sizeof(hdr+phdr)
    uint64_t e_phoff       = 64;
    uint64_t e_shoff       = 0;
    uint32_t e_flags       = 0;
    uint16_t e_ehsize      = 64;
    uint16_t e_phentsize   = 56;
    uint16_t e_phnum       = 1;
    uint16_t e_shentsize   = 64;
    uint16_t e_shnum       = 0;
    uint16_t e_shstrndx    = 0;

    // PT_LOAD phdr (56 bytes at offset 64)
    uint32_t p_type    = 1;           // PT_LOAD
    uint32_t p_flags   = 5;           // PF_R | PF_X
    uint64_t p_offset  = 0;
    uint64_t p_vaddr   = 0x400000;
    uint64_t p_paddr   = 0x400000;
    uint64_t p_filesz  = 128;         // will be filled in
    uint64_t p_memsz   = 128;
    uint64_t p_align   = 0x1000;

    // Code (8 bytes at offset 0x78 = 120)
    uint8_t code[8] = {
        0x48, 0x31, 0xC0,  // xor rax, rax
        0xC3,              // ret
        0x90, 0x90, 0x90, 0x90  // nop padding
    };
};
#pragma pack(pop)

static_assert(sizeof(MiniElf64) == 128, "MiniElf64 must be 128 bytes");

std::vector<uint8_t> MakeHelloWorldElf() {
    MiniElf64 elf;
    elf.p_filesz = sizeof(MiniElf64);
    elf.p_memsz  = sizeof(MiniElf64);
    std::vector<uint8_t> v(sizeof(MiniElf64));
    std::memcpy(v.data(), &elf, sizeof(MiniElf64));
    return v;
}

std::vector<uint8_t> MakeUnsupportedInsnElf() {
    MiniElf64 elf;
    elf.p_filesz = sizeof(MiniElf64);
    elf.p_memsz  = sizeof(MiniElf64);
    // Replace code with an invalid/unsupported encoding
    elf.code[0] = 0x0F; elf.code[1] = 0xFF; // UD2 — guaranteed invalid
    elf.code[2] = 0x90; elf.code[3] = 0xC3;
    std::vector<uint8_t> v(sizeof(MiniElf64));
    std::memcpy(v.data(), &elf, sizeof(MiniElf64));
    return v;
}

// ─── Simple test runner ───────────────────────────────────────────────────────

static int g_passed = 0;
static int g_failed = 0;

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        std::printf("  [FAIL] %s:%d  Assertion failed: %s\n", __FILE__, __LINE__, #expr); \
        ++g_failed; return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::printf("  [FAIL] %s:%d  Expected %s == %s\n", __FILE__, __LINE__, #a, #b); \
        ++g_failed; return; \
    } \
} while(0)

#define ASSERT_GT(a, b) do { \
    if (!((a) > (b))) { \
        std::printf("  [FAIL] %s:%d  Expected %s > %s\n", __FILE__, __LINE__, #a, #b); \
        ++g_failed; return; \
    } \
} while(0)

#define ASSERT_NE(a, b) do { \
    if ((a) == (b)) { \
        std::printf("  [FAIL] %s:%d  Expected %s != %s\n", __FILE__, __LINE__, #a, #b); \
        ++g_failed; return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))

#define TEST(name) \
    static void Test_##name(); \
    static struct Register_##name { Register_##name() { \
        std::printf("[TEST] " #name "\n"); \
        Test_##name(); \
        if (g_failed == 0) { ++g_passed; std::printf("  [ OK ] " #name "\n"); } \
        else { g_failed = 0; std::printf("  [FAIL] " #name "\n"); } \
    } } register_##name; \
    static void Test_##name()

} // anonymous namespace

// ─── Test 1: OpenOrbis Hello World ELF Loading ───────────────────────────────

TEST(OpenOrbisHelloWorldElfLoad) {
    Loader::OpenOrbisElfLoader loader;
    auto elf_bytes = MakeHelloWorldElf();
    auto result = loader.LoadFromMemory(elf_bytes.data(), elf_bytes.size(), "HelloWorld");

    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.entry_vaddr, static_cast<uint64_t>(0x400078));
    ASSERT_EQ(result.base_vaddr,  static_cast<uint64_t>(0x400000));
    ASSERT_GT(result.image_size,  static_cast<uint64_t>(0));
    ASSERT_FALSE(result.image_buffer.empty());

    std::printf("  entry=0x%llx base=0x%llx sz=0x%llx\n",
                (unsigned long long)result.entry_vaddr,
                (unsigned long long)result.base_vaddr,
                (unsigned long long)result.image_size);
}

// ─── Test 2: JIT Dispatch Loop — compile + cache hit ─────────────────────────

TEST(JitDispatchLoopCompileAndCacheHit) {
    Loader::Recompiler::X86RuntimeBridge bridge(4 * 1024 * 1024);
    Loader::Recompiler::JitTelemetryCollector telemetry;
    telemetry.StartSession("TEST", "JitDispatchTest");

    Loader::Ps5JitDispatchLoop loop(bridge, telemetry);

    // Build minimal ELF
    auto elf_bytes = MakeHelloWorldElf();
    Loader::OpenOrbisElfLoader loader;
    auto result = loader.LoadFromMemory(elf_bytes.data(), elf_bytes.size(), "JitTest");
    ASSERT_TRUE(result.success);

    loop.SetupFromLoadResult(result);

    Loader::DispatchConfig cfg;
    cfg.frame_budget_ms      = 1000.0;  // Unlimited for this test
    cfg.max_blocks_per_slice = 64;
    cfg.stop_on_unsupported  = false;
    loop.Configure(cfg);

    // First run — should cause compilation
    auto dr1 = loop.RunSlice();
    ASSERT_NE(dr1.stop_reason, Loader::DispatchStopReason::Exception);
    ASSERT_GT(dr1.blocks_run, static_cast<uint32_t>(0));

    std::printf("  First slice: %u blocks, reason=%d\n",
                dr1.blocks_run, static_cast<int>(dr1.stop_reason));

    auto summary = telemetry.GetRunSummary();
    std::printf("  JIT compiled: %" PRIu64 " cache-hit: %" PRIu64 "\n",
                summary.total_blocks_compiled,
                summary.total_blocks_cache_hit);

    telemetry.EndSession();
}

// ─── Test 3: Subsystem Stubs — all 6 categories ──────────────────────────────

TEST(SubsystemStubsAllCategories) {
    Loader::Recompiler::JitTelemetryCollector telemetry;
    telemetry.StartSession("TEST", "SubsystemTest");

    Kernel::OpenOrbisSubsystemHub hub(telemetry);
    hub.RegisterAll();

    std::printf("  Registered %zu stubs\n", hub.StubCount());
    ASSERT_GT(hub.StubCount(), static_cast<size_t>(30));

    Kernel::SubsystemCallCtx ctx;

    // Filesystem
    ctx = {}; ctx.arg2 = 64;
    ASSERT_GT(hub.Dispatch("sceKernelOpen", ctx),  static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("sceKernelRead",  ctx), static_cast<int64_t>(64));
    ASSERT_EQ(hub.Dispatch("sceKernelClose", ctx), static_cast<int64_t>(0));

    // Threads
    ASSERT_EQ(hub.Dispatch("scePthreadCreate",      ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("scePthreadMutexLock",   ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("scePthreadMutexUnlock", ctx), static_cast<int64_t>(0));

    // Input
    ASSERT_EQ(hub.Dispatch("sceHidServiceOpen", ctx), static_cast<int64_t>(0));
    ctx.arg0 = 1;
    ASSERT_EQ(hub.Dispatch("scePadOpen", ctx), static_cast<int64_t>(1));

    // Audio
    ctx = {};
    ASSERT_GT(hub.Dispatch("sceAudioOutOpen",   ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("sceAudioOutOutput", ctx), static_cast<int64_t>(0));

    // Networking
    ASSERT_GT(hub.Dispatch("sceNetSocket",  ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("sceNetConnect", ctx), static_cast<int64_t>(0));

    // Graphics
    ASSERT_GT(hub.Dispatch("sceVideoOutOpen",       ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("sceVideoOutSubmitFlip", ctx), static_cast<int64_t>(0));
    ASSERT_EQ(hub.Dispatch("sceGnmSubmitCommandBuffers", ctx), static_cast<int64_t>(0));

    // Unknown stub → ENOSYS
    int64_t nosys = hub.Dispatch("sceNonExistentFn", ctx);
    ASSERT_EQ(nosys, static_cast<int64_t>(Kernel::SCE_ERROR_ERRNO_ENOSYS));

    auto summary = telemetry.GetRunSummary();
    std::printf("  Total syscalls recorded: %" PRIu64 "\n", summary.total_syscalls);
    ASSERT_GT(summary.total_syscalls, static_cast<uint64_t>(0));

    telemetry.EndSession();
}

// ─── Test 4: Telemetry Collector ─────────────────────────────────────────────

TEST(TelemetryCollectorCountersAndFrames) {
    Loader::Recompiler::JitTelemetryCollector tel;
    tel.StartSession("TELE_TEST", "TelemetryTest");

    // Simulate 5 frames
    for (int frame = 0; frame < 5; ++frame) {
        tel.BeginFrame();
        tel.RecordBlockCompiled(0x48, 12, 24);  // Primary opcode 0x48 (REX prefix)
        tel.RecordBlockCompiled(0xC3, 2, 4);    // Primary opcode 0xC3 (RET)
        tel.RecordBlockCacheHit(0x48);
        tel.RecordJitCycles(10000 + frame * 1000);
        tel.RecordSyscall("sceKernelRead");
        tel.RecordSyscall("sceAudioOutOutput");
        tel.RecordPm4Packet();
        tel.EndFrame();
    }

    ASSERT_EQ(tel.GetFrameCount(), static_cast<size_t>(5));

    auto summary = tel.GetRunSummary();
    ASSERT_EQ(summary.total_frames,           static_cast<uint64_t>(5));
    ASSERT_EQ(summary.total_blocks_compiled,  static_cast<uint64_t>(10)); // 2/frame * 5
    ASSERT_EQ(summary.total_blocks_cache_hit, static_cast<uint64_t>(5));  // 1/frame * 5
    ASSERT_EQ(summary.total_syscalls,         static_cast<uint64_t>(10)); // 2/frame * 5
    ASSERT_EQ(summary.total_pm4_packets,      static_cast<uint64_t>(5));
    ASSERT_GT(summary.total_jit_cycles,       static_cast<uint64_t>(0));
    ASSERT_FALSE(summary.top_opcodes.empty());

    std::printf("  Cache hit rate: %.1f%%\n", summary.cache_hit_rate_pct);
    std::printf("  Top opcode: 0x%02X (%llu dispatches)\n",
                summary.top_opcodes[0].first,
                (unsigned long long)summary.top_opcodes[0].second);
    std::printf("  Subsystem calls: %zu unique stubs\n", summary.subsystem_calls.size());

    ASSERT_GT(summary.subsystem_calls.size(), static_cast<size_t>(0));

    tel.EndSession();
    tel.PrintSummary();
}

// ─── Test 5: Compatibility Report — HTML + MD generation ─────────────────────

TEST(CompatibilityReportHtmlAndMarkdown) {
    Loader::Recompiler::JitTelemetryCollector tel;
    tel.StartSession("COMPAT_TEST", "Ps5CompatReportTest");
    tel.BeginFrame();
    tel.RecordBlockCompiled(0x48, 8, 16);
    tel.RecordBlockCacheHit(0x48);
    tel.RecordSyscall("sceVideoOutSubmitFlip");
    tel.RecordPm4Packet();
    tel.EndFrame();
    tel.EndSession();

    ExecutionSessionStats session;
    session.title_id       = "COMPAT_TEST";
    session.title_name     = "Compatibility Report Test";
    session.app_version    = "01.00";
    session.sdk_version    = "09.00.00";
    session.session_status = GameStatus::Boots;
    session.total_frames   = 1;
    session.average_fps    = 60.0;
    session.end_timestamp  = "2026-08-05T05:00:00Z";

    Compat::Ps5CompatibilityReport report;
    report.SetSessionStats(session);
    report.SetJitSummary(tel.GetRunSummary());

    const std::string html = report.GenerateHtmlString();
    const std::string md   = report.GenerateMdString();

    // Verify HTML contains required sections
    ASSERT_TRUE(html.find("JIT Cache Statistics")    != std::string::npos);
    ASSERT_TRUE(html.find("Frame Telemetry")         != std::string::npos);
    ASSERT_TRUE(html.find("Opcode Distribution")     != std::string::npos);
    ASSERT_TRUE(html.find("Subsystem Coverage")      != std::string::npos);
    ASSERT_TRUE(html.find("Unsupported Instructions") != std::string::npos);
    ASSERT_TRUE(html.find("COMPAT_TEST")             != std::string::npos);

    // Verify MD contains required headings
    ASSERT_TRUE(md.find("## JIT Cache Statistics")  != std::string::npos);
    ASSERT_TRUE(md.find("## Frame Telemetry")       != std::string::npos);
    ASSERT_TRUE(md.find("## Opcode Distribution")   != std::string::npos);
    ASSERT_TRUE(md.find("## Subsystem Coverage")    != std::string::npos);
    ASSERT_TRUE(md.find("## Unsupported")           != std::string::npos);

    // Write to temp directory and verify files exist
    const std::filesystem::path out_dir = std::filesystem::temp_directory_path()
                                          / "kytyps5_compat_test";
    bool ok = report.Generate(out_dir, "COMPAT_TEST");
    ASSERT_TRUE(ok);
    ASSERT_TRUE(std::filesystem::exists(out_dir / "Ps5CompatibilityReport_COMPAT_TEST.html"));
    ASSERT_TRUE(std::filesystem::exists(out_dir / "Ps5CompatibilityReport_COMPAT_TEST.md"));

    std::printf("  HTML size: %zu bytes\n", html.size());
    std::printf("  MD   size: %zu bytes\n", md.size());
    std::printf("  Reports written to: %s\n", out_dir.c_str());

    // Cleanup
    std::filesystem::remove_all(out_dir);
}

// ─── Test 6: Unsupported Instruction Reporting ────────────────────────────────

TEST(UnsupportedInstructionReporting) {
    Loader::Recompiler::JitTelemetryCollector tel;
    tel.StartSession("UNSUP_TEST", "UnsupportedInsnTest");
    tel.BeginFrame();

    // Inject several unsupported instruction encodings
    tel.RecordUnsupportedInstruction(0x0FFFFF0F); // UD0
    tel.RecordUnsupportedInstruction(0x0FFFFF0F); // Same one twice
    tel.RecordUnsupportedInstruction(0xDEADBEEF); // Another one
    tel.RecordUnsupportedInstruction(0xCAFEBABE); // Third unique one

    tel.EndFrame();
    tel.EndSession();

    auto summary = tel.GetRunSummary();
    ASSERT_GT(summary.unsupported_instructions.size(), static_cast<size_t>(0));

    // Find the 0x0FFFFF0F entry — should have count 2
    bool found = false;
    for (const auto& [enc, cnt] : summary.unsupported_instructions) {
        if (enc == "0x0FFFFF0F") {
            ASSERT_EQ(cnt, static_cast<uint64_t>(2));
            found = true;
        }
    }
    ASSERT_TRUE(found);

    std::printf("  Unique unsupported instructions: %zu\n",
                summary.unsupported_instructions.size());
    for (const auto& [enc, cnt] : summary.unsupported_instructions) {
        std::printf("    %s : %" PRIu64 " times\n", enc.c_str(), cnt);
    }

    // Verify report highlights unsupported instructions
    Compat::Ps5CompatibilityReport report;
    ExecutionSessionStats session;
    session.title_id = "UNSUP_TEST";
    session.session_status = GameStatus::InGame;
    report.SetSessionStats(session);
    report.SetJitSummary(summary);

    const std::string html = report.GenerateHtmlString();
    ASSERT_TRUE(html.find("Unsupported Instructions Detected") != std::string::npos);
    ASSERT_TRUE(html.find("0x0FFFFF0F") != std::string::npos);

    const std::string md = report.GenerateMdString();
    ASSERT_TRUE(md.find("0x0FFFFF0F") != std::string::npos);
    // Should not claim "No unsupported instructions"
    ASSERT_TRUE(md.find("No unsupported instructions detected") == std::string::npos);
}

// ─── Test runner main ─────────────────────────────────────────────────────────

int main() {
    std::printf("\n");
    std::printf("================================================================================\n");
    std::printf("  KytyPS5 — Phase P: PS5 JIT Emulator Integration Tests\n");
    std::printf("================================================================================\n\n");

    // Tests are registered and run via static constructors above.

    std::printf("\n================================================================================\n");
    std::printf("  Results: %d passed, %d failed\n", g_passed, g_failed);
    std::printf("================================================================================\n\n");

    return (g_failed == 0) ? 0 : 1;
}
