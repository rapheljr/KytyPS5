// jitTelemetryCollector.h
//
// Lock-free performance telemetry collector for the PS5 ARM64 JIT dispatcher.
//
// Tracks per-frame and per-run metrics:
//   - JIT block compilation vs cache-hit rates
//   - IR/ARM64 instruction volume
//   - Opcode distribution histogram (256 buckets for x86 primary opcodes)
//   - Unsupported / unimplemented instruction set
//   - Frame timing statistics
//   - CPU cycle estimates (ARM64 CNTVCT_EL0 / host clock)

#ifndef LOADER_RECOMPILER_JIT_TELEMETRY_COLLECTOR_H
#define LOADER_RECOMPILER_JIT_TELEMETRY_COLLECTOR_H

#include "common/common.h"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace Loader::Recompiler {

// ─── Per-frame snapshot ───────────────────────────────────────────────────────

struct FrameTelemetrySnapshot {
    uint64_t frame_index         = 0;
    uint32_t blocks_compiled     = 0;  // New translations this frame
    uint32_t blocks_cache_hit    = 0;  // Served from RadixCodeCache
    uint32_t blocks_cache_miss   = 0;  // Not found, triggered compilation
    uint32_t ir_instructions     = 0;  // IR instructions lowered
    uint32_t arm64_instructions  = 0;  // ARM64 instructions emitted
    uint32_t syscalls_dispatched = 0;  // Syscall stub invocations
    uint32_t pm4_packets         = 0;  // GPU command packets submitted
    uint64_t jit_cycles          = 0;  // Cumulative JIT dispatch cycles
    double   frame_time_ms       = 0.0;
};

// ─── Per-run summary ──────────────────────────────────────────────────────────

struct JitRunSummary {
    std::string title_id;
    std::string module_name;

    uint64_t total_frames            = 0;
    uint64_t total_blocks_compiled   = 0;
    uint64_t total_blocks_cache_hit  = 0;
    uint64_t total_blocks_evicted    = 0;
    uint64_t total_ir_instructions   = 0;
    uint64_t total_arm64_instructions = 0;
    uint64_t total_syscalls          = 0;
    uint64_t total_pm4_packets       = 0;
    uint64_t total_jit_cycles        = 0;

    double avg_frame_time_ms   = 0.0;
    double min_frame_time_ms   = 0.0;
    double max_frame_time_ms   = 0.0;
    double avg_fps             = 0.0;
    double cache_hit_rate_pct  = 0.0;   // hit / (hit + miss) * 100
    double cache_evict_rate_pct = 0.0;  // evict / compiled * 100

    // Top-20 hottest x86 primary opcodes (opcode, count)
    std::vector<std::pair<uint8_t, uint64_t>> top_opcodes;

    // Unsupported instructions: encoding (as hex string) → count
    std::vector<std::pair<std::string, uint64_t>> unsupported_instructions;

    // Subsystem call counts: stub name → invocation count
    std::vector<std::pair<std::string, uint64_t>> subsystem_calls;
};

// ─── Collector class ──────────────────────────────────────────────────────────

class JitTelemetryCollector {
public:
    JitTelemetryCollector();
    ~JitTelemetryCollector() = default;

    KYTY_CLASS_NO_COPY(JitTelemetryCollector);

    // ── Session control ──────────────────────────────────────────────────────
    void StartSession(const std::string& title_id, const std::string& module_name);
    void EndSession();

    // ── Frame boundary markers ───────────────────────────────────────────────
    void BeginFrame();
    void EndFrame();   // Finalises frame snapshot, appends to m_frames

    // ── Per-dispatch counters (hot path — all atomic) ────────────────────────
    void RecordBlockCompiled(uint8_t primary_opcode, uint32_t ir_count, uint32_t arm64_count);
    void RecordBlockCacheHit(uint8_t primary_opcode);
    void RecordBlockCacheEvicted();
    void RecordUnsupportedInstruction(uint32_t encoding);
    void RecordJitCycles(uint64_t cycles);

    // ── Subsystem / GPU side-channel ────────────────────────────────────────
    void RecordSyscall(const std::string& stub_name);
    void RecordPm4Packet();

    // ── Queries ──────────────────────────────────────────────────────────────
    [[nodiscard]] JitRunSummary          GetRunSummary() const;
    [[nodiscard]] FrameTelemetrySnapshot GetLastFrameSnapshot() const;
    [[nodiscard]] size_t                 GetFrameCount() const noexcept {
        return m_frame_count.load(std::memory_order_relaxed);
    }

    // Pretty-print summary to stdout
    void PrintSummary() const;

private:
    // Atomic run-level counters
    std::atomic<uint64_t> m_total_compiled{0};
    std::atomic<uint64_t> m_total_cache_hit{0};
    std::atomic<uint64_t> m_total_cache_miss{0};
    std::atomic<uint64_t> m_total_evicted{0};
    std::atomic<uint64_t> m_total_ir_instr{0};
    std::atomic<uint64_t> m_total_arm64_instr{0};
    std::atomic<uint64_t> m_total_syscalls{0};
    std::atomic<uint64_t> m_total_pm4_packets{0};
    std::atomic<uint64_t> m_total_jit_cycles{0};
    std::atomic<uint64_t> m_frame_count{0};

    // 256-bucket opcode histogram (one slot per x86 primary byte)
    std::array<std::atomic<uint64_t>, 256> m_opcode_histogram;

    // Unsupported instructions — protected by mutex (cold path)
    mutable std::mutex                          m_unsupported_mutex;
    std::vector<std::pair<uint32_t, uint64_t>>  m_unsupported_instructions; // {encoding, count}

    // Subsystem call counts — protected by mutex (cold-ish path)
    mutable std::mutex                          m_syscall_mutex;
    std::vector<std::pair<std::string, uint64_t>> m_syscall_counts; // {name, count}

    // Frame-level accumulators (single producer — dispatch thread)
    struct FrameAccumulator {
        uint32_t blocks_compiled    = 0;
        uint32_t blocks_cache_hit   = 0;
        uint32_t blocks_cache_miss  = 0;
        uint32_t ir_instructions    = 0;
        uint32_t arm64_instructions = 0;
        uint32_t syscalls           = 0;
        uint32_t pm4_packets        = 0;
        uint64_t jit_cycles         = 0;
        std::chrono::steady_clock::time_point frame_start;
    };
    FrameAccumulator m_current_frame;

    // Frame history (guarded by m_frames_mutex)
    mutable std::mutex                    m_frames_mutex;
    std::vector<FrameTelemetrySnapshot>   m_frame_snapshots;

    std::string m_title_id;
    std::string m_module_name;
    bool        m_session_active = false;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_TELEMETRY_COLLECTOR_H
