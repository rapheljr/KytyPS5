// jitTelemetryCollector.cpp
//
// JIT Telemetry Collector Implementation.

#include "loader/recompiler/jitTelemetryCollector.h"

#include "common/logging/log.h"

#include <algorithm>
#include <cstdio>
#include <cinttypes>

namespace Loader::Recompiler {

JitTelemetryCollector::JitTelemetryCollector() {
    for (auto& counter : m_opcode_histogram) {
        counter.store(0, std::memory_order_relaxed);
    }
}

// ─── Session control ──────────────────────────────────────────────────────────

void JitTelemetryCollector::StartSession(const std::string& title_id,
                                          const std::string& module_name) {
    m_title_id      = title_id;
    m_module_name   = module_name;
    m_session_active = true;

    // Reset all run-level counters
    m_total_compiled.store(0);
    m_total_cache_hit.store(0);
    m_total_cache_miss.store(0);
    m_total_evicted.store(0);
    m_total_ir_instr.store(0);
    m_total_arm64_instr.store(0);
    m_total_syscalls.store(0);
    m_total_pm4_packets.store(0);
    m_total_jit_cycles.store(0);
    m_frame_count.store(0);

    for (auto& c : m_opcode_histogram) c.store(0);

    {
        std::lock_guard<std::mutex> lock(m_unsupported_mutex);
        m_unsupported_instructions.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_syscall_mutex);
        m_syscall_counts.clear();
    }
    {
        std::lock_guard<std::mutex> lock(m_frames_mutex);
        m_frame_snapshots.clear();
    }

    m_current_frame = FrameAccumulator{};
}

void JitTelemetryCollector::EndSession() {
    m_session_active = false;
}

// ─── Frame boundaries ─────────────────────────────────────────────────────────

void JitTelemetryCollector::BeginFrame() {
    m_current_frame = FrameAccumulator{};
    m_current_frame.frame_start = std::chrono::steady_clock::now();
}

void JitTelemetryCollector::EndFrame() {
    auto now      = std::chrono::steady_clock::now();
    double dt_ms  = std::chrono::duration<double, std::milli>(
                        now - m_current_frame.frame_start).count();

    const uint64_t idx = m_frame_count.fetch_add(1, std::memory_order_relaxed);

    FrameTelemetrySnapshot snap;
    snap.frame_index         = idx;
    snap.blocks_compiled     = m_current_frame.blocks_compiled;
    snap.blocks_cache_hit    = m_current_frame.blocks_cache_hit;
    snap.blocks_cache_miss   = m_current_frame.blocks_cache_miss;
    snap.ir_instructions     = m_current_frame.ir_instructions;
    snap.arm64_instructions  = m_current_frame.arm64_instructions;
    snap.syscalls_dispatched = m_current_frame.syscalls;
    snap.pm4_packets         = m_current_frame.pm4_packets;
    snap.jit_cycles          = m_current_frame.jit_cycles;
    snap.frame_time_ms       = dt_ms;

    {
        std::lock_guard<std::mutex> lock(m_frames_mutex);
        m_frame_snapshots.push_back(snap);
    }
}

// ─── Hot-path counters ────────────────────────────────────────────────────────

void JitTelemetryCollector::RecordBlockCompiled(uint8_t primary_opcode,
                                                 uint32_t ir_count,
                                                 uint32_t arm64_count) {
    m_total_compiled.fetch_add(1, std::memory_order_relaxed);
    m_total_cache_miss.fetch_add(1, std::memory_order_relaxed);
    m_total_ir_instr.fetch_add(ir_count, std::memory_order_relaxed);
    m_total_arm64_instr.fetch_add(arm64_count, std::memory_order_relaxed);
    m_opcode_histogram[primary_opcode].fetch_add(1, std::memory_order_relaxed);

    m_current_frame.blocks_compiled++;
    m_current_frame.blocks_cache_miss++;
    m_current_frame.ir_instructions    += ir_count;
    m_current_frame.arm64_instructions += arm64_count;
}

void JitTelemetryCollector::RecordBlockCacheHit(uint8_t primary_opcode) {
    m_total_cache_hit.fetch_add(1, std::memory_order_relaxed);
    m_opcode_histogram[primary_opcode].fetch_add(1, std::memory_order_relaxed);

    m_current_frame.blocks_cache_hit++;
}

void JitTelemetryCollector::RecordBlockCacheEvicted() {
    m_total_evicted.fetch_add(1, std::memory_order_relaxed);
}

void JitTelemetryCollector::RecordUnsupportedInstruction(uint32_t encoding) {
    std::lock_guard<std::mutex> lock(m_unsupported_mutex);
    for (auto& [enc, cnt] : m_unsupported_instructions) {
        if (enc == encoding) {
            ++cnt;
            return;
        }
    }
    m_unsupported_instructions.emplace_back(encoding, 1u);
}

void JitTelemetryCollector::RecordJitCycles(uint64_t cycles) {
    m_total_jit_cycles.fetch_add(cycles, std::memory_order_relaxed);
    m_current_frame.jit_cycles += cycles;
}

void JitTelemetryCollector::RecordSyscall(const std::string& stub_name) {
    m_total_syscalls.fetch_add(1, std::memory_order_relaxed);
    m_current_frame.syscalls++;

    std::lock_guard<std::mutex> lock(m_syscall_mutex);
    for (auto& [name, cnt] : m_syscall_counts) {
        if (name == stub_name) { ++cnt; return; }
    }
    m_syscall_counts.emplace_back(stub_name, 1u);
}

void JitTelemetryCollector::RecordPm4Packet() {
    m_total_pm4_packets.fetch_add(1, std::memory_order_relaxed);
    m_current_frame.pm4_packets++;
}

// ─── Queries ──────────────────────────────────────────────────────────────────

FrameTelemetrySnapshot JitTelemetryCollector::GetLastFrameSnapshot() const {
    std::lock_guard<std::mutex> lock(m_frames_mutex);
    if (m_frame_snapshots.empty()) return {};
    return m_frame_snapshots.back();
}

JitRunSummary JitTelemetryCollector::GetRunSummary() const {
    JitRunSummary s;
    s.title_id    = m_title_id;
    s.module_name = m_module_name;

    s.total_frames             = m_frame_count.load();
    s.total_blocks_compiled    = m_total_compiled.load();
    s.total_blocks_cache_hit   = m_total_cache_hit.load();
    s.total_blocks_evicted     = m_total_evicted.load();
    s.total_ir_instructions    = m_total_ir_instr.load();
    s.total_arm64_instructions = m_total_arm64_instr.load();
    s.total_syscalls           = m_total_syscalls.load();
    s.total_pm4_packets        = m_total_pm4_packets.load();
    s.total_jit_cycles         = m_total_jit_cycles.load();

    const uint64_t total_dispatches = s.total_blocks_compiled + s.total_blocks_cache_hit;
    s.cache_hit_rate_pct = total_dispatches > 0
        ? (100.0 * static_cast<double>(s.total_blocks_cache_hit) /
           static_cast<double>(total_dispatches))
        : 0.0;
    s.cache_evict_rate_pct = s.total_blocks_compiled > 0
        ? (100.0 * static_cast<double>(s.total_blocks_evicted) /
           static_cast<double>(s.total_blocks_compiled))
        : 0.0;

    // Build opcode top-20
    std::vector<std::pair<uint8_t, uint64_t>> opc_vec;
    opc_vec.reserve(256);
    for (int i = 0; i < 256; ++i) {
        uint64_t cnt = m_opcode_histogram[i].load(std::memory_order_relaxed);
        if (cnt > 0) opc_vec.emplace_back(static_cast<uint8_t>(i), cnt);
    }
    std::sort(opc_vec.begin(), opc_vec.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });
    if (opc_vec.size() > 20) opc_vec.resize(20);
    s.top_opcodes = std::move(opc_vec);

    // Unsupported instructions
    {
        std::lock_guard<std::mutex> lock(m_unsupported_mutex);
        for (const auto& [enc, cnt] : m_unsupported_instructions) {
            char hex[12];
            std::snprintf(hex, sizeof(hex), "0x%08X", enc);
            s.unsupported_instructions.emplace_back(hex, cnt);
        }
    }
    std::sort(s.unsupported_instructions.begin(), s.unsupported_instructions.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    // Syscall counts
    {
        std::lock_guard<std::mutex> lock(m_syscall_mutex);
        s.subsystem_calls = m_syscall_counts;
    }
    std::sort(s.subsystem_calls.begin(), s.subsystem_calls.end(),
              [](const auto& a, const auto& b){ return a.second > b.second; });

    // Frame timing
    {
        std::lock_guard<std::mutex> lock(m_frames_mutex);
        if (!m_frame_snapshots.empty()) {
            double sum = 0.0;
            double mn  = m_frame_snapshots[0].frame_time_ms;
            double mx  = mn;
            for (const auto& f : m_frame_snapshots) {
                sum += f.frame_time_ms;
                mn   = std::min(mn, f.frame_time_ms);
                mx   = std::max(mx, f.frame_time_ms);
            }
            s.avg_frame_time_ms = sum / static_cast<double>(m_frame_snapshots.size());
            s.min_frame_time_ms = mn;
            s.max_frame_time_ms = mx;
            s.avg_fps = s.avg_frame_time_ms > 0.0
                ? 1000.0 / s.avg_frame_time_ms : 0.0;
        }
    }

    return s;
}

void JitTelemetryCollector::PrintSummary() const {
    const auto s = GetRunSummary();
    std::printf("\n");
    std::printf("=============================================================================\n");
    std::printf("  JIT Telemetry Run Summary — %s [%s]\n",
                s.title_id.c_str(), s.module_name.c_str());
    std::printf("=============================================================================\n");
    std::printf("  Frames             : %" PRIu64 "\n", s.total_frames);
    std::printf("  Avg FPS            : %.1f\n", s.avg_fps);
    std::printf("  Frame time (avg)   : %.2f ms\n", s.avg_frame_time_ms);
    std::printf("  Frame time (min)   : %.2f ms\n", s.min_frame_time_ms);
    std::printf("  Frame time (max)   : %.2f ms\n", s.max_frame_time_ms);
    std::printf("-----------------------------------------------------------------------------\n");
    std::printf("  Blocks compiled    : %" PRIu64 "\n", s.total_blocks_compiled);
    std::printf("  Cache hits         : %" PRIu64 " (%.1f%%)\n",
                s.total_blocks_cache_hit, s.cache_hit_rate_pct);
    std::printf("  Cache evictions    : %" PRIu64 " (%.1f%%)\n",
                s.total_blocks_evicted, s.cache_evict_rate_pct);
    std::printf("  IR instructions    : %" PRIu64 "\n", s.total_ir_instructions);
    std::printf("  ARM64 instructions : %" PRIu64 "\n", s.total_arm64_instructions);
    std::printf("  JIT cycles (est.)  : %" PRIu64 "\n", s.total_jit_cycles);
    std::printf("  Syscalls dispatched: %" PRIu64 "\n", s.total_syscalls);
    std::printf("  PM4 packets        : %" PRIu64 "\n", s.total_pm4_packets);
    std::printf("-----------------------------------------------------------------------------\n");

    if (!s.top_opcodes.empty()) {
        std::printf("  Top x86 opcodes:\n");
        for (const auto& [opc, cnt] : s.top_opcodes) {
            std::printf("    0x%02X  : %" PRIu64 " dispatches\n", opc, cnt);
        }
    }

    if (!s.unsupported_instructions.empty()) {
        std::printf("  Unsupported instructions:\n");
        for (const auto& [enc, cnt] : s.unsupported_instructions) {
            std::printf("    %s : %" PRIu64 " times\n", enc.c_str(), cnt);
        }
    }

    if (!s.subsystem_calls.empty()) {
        std::printf("  Subsystem calls:\n");
        for (const auto& [name, cnt] : s.subsystem_calls) {
            std::printf("    %-40s : %" PRIu64 "\n", name.c_str(), cnt);
        }
    }
    std::printf("=============================================================================\n\n");
}

} // namespace Loader::Recompiler
