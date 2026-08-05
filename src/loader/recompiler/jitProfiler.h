// jitProfiler.h
//
// Comprehensive ARM64 JIT Profiler & Optimization Advisor for KytyPS5.
//
// Collects & analyzes:
//   - Hot / Cold block metrics (Guest RIP, execution count, latency, reuse ratio)
//   - Translation latency breakdown (Lowering, IR Opt, RegAlloc, Codegen)
//   - Opcode frequency & SIMD vectorization ratio
//   - Hardware/Cache metrics (Cache hit rate, fragmentation %, branch prediction, memory stalls)
//   - SVG Flame Graph visualization of JIT translation and execution
//   - Ranked Optimization Advisor (ranked by expected speedup)
//   - Multi-format exporters: HTML dashboard, CSV tables, JSON payload

#ifndef LOADER_RECOMPILER_JIT_PROFILER_H
#define LOADER_RECOMPILER_JIT_PROFILER_H

#include "common/common.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

// ─── Block Profile Entry ──────────────────────────────────────────────────────

struct BlockProfile {
    uint64_t guest_rip          = 0;
    size_t   size_bytes         = 0;
    uint64_t execution_count    = 0;
    uint64_t total_exec_time_ns = 0;
    uint64_t translation_time_ns = 0;
    uint32_t ir_instruction_cnt = 0;
    uint32_t arm64_instruction_cnt = 0;
    uint32_t simd_instruction_cnt  = 0;
    uint32_t branch_instruction_cnt = 0;
    uint32_t memory_inst_count     = 0;
    uint64_t memory_stall_cycles   = 0;
    uint64_t branch_mispredicts    = 0;
    bool     is_linked             = false;
};

// ─── Translation Sub-Phase Breakdown ──────────────────────────────────────────

struct TranslationPhaseMetrics {
    uint64_t lowering_time_ns = 0;
    uint64_t ir_opt_time_ns   = 0;
    uint64_t reg_alloc_time_ns = 0;
    uint64_t codegen_time_ns  = 0;
    uint64_t total_time_ns    = 0;
};

// ─── Ranked Optimization Opportunity ──────────────────────────────────────────

struct OptimizationRecommendation {
    uint32_t    rank = 0;
    std::string category;              // "SIMD Vectorization", "Block Linking", "Cache Eviction", "Lowering Opt"
    std::string title;
    std::string description;
    double      expected_speedup_x = 1.0; // e.g. 1.35x speedup estimate
    std::string target_rip_hex;
};

// ─── Full Profile Summary Data ────────────────────────────────────────────────

struct JitProfileSummary {
    std::string title_id;
    std::string module_name;

    // Block statistics
    size_t   total_unique_blocks = 0;
    size_t   hot_block_count     = 0;  // Executed >= hot threshold (e.g. 100)
    size_t   cold_block_count    = 0; // Executed == 1
    uint64_t total_executions    = 0;
    double   block_reuse_ratio   = 0.0; // total_executions / total_unique_blocks

    // Latency metrics
    double avg_exec_latency_ns    = 0.0;
    double max_exec_latency_ns    = 0.0;
    double avg_trans_latency_ns   = 0.0;
    TranslationPhaseMetrics phase_breakdown;

    // Cache & Hardware metrics
    double cache_hit_rate_pct    = 0.0;
    double cache_fragmentation_pct = 0.0;
    double branch_prediction_pct = 0.0;
    double simd_usage_pct        = 0.0;
    uint64_t total_memory_stalls = 0;

    // Top hot blocks sorted by execution count / latency
    std::vector<BlockProfile> hot_blocks;
    std::vector<BlockProfile> cold_blocks;

    // Opcode histogram: opcode byte / mnemonic -> count
    std::vector<std::pair<std::string, uint64_t>> top_opcodes;

    // Optimization Advisor recommendations
    std::vector<OptimizationRecommendation> recommendations;
};

// ─── JitProfiler Class ────────────────────────────────────────────────────────

class JitProfiler {
public:
    JitProfiler() = default;
    ~JitProfiler() = default;

    KYTY_CLASS_NO_COPY(JitProfiler);

    // Session Management
    void StartProfiling(const std::string& title_id, const std::string& module_name);
    void StopProfiling();

    // Hot-path metric recording
    void RecordBlockTranslation(uint64_t guest_rip, size_t size_bytes,
                                const TranslationPhaseMetrics& phases,
                                uint32_t ir_cnt, uint32_t arm64_cnt,
                                uint32_t simd_cnt, uint32_t branch_cnt, uint32_t mem_cnt);

    void RecordBlockExecution(uint64_t guest_rip, uint64_t exec_time_ns,
                              uint64_t stall_cycles = 0, uint64_t mispredicts = 0);

    void RecordCacheHit();
    void RecordCacheMiss();
    void RecordBlockLinked(uint64_t src_rip, uint64_t dst_rip);
    void RecordOpcode(const std::string& opcode_name, uint64_t count = 1);
    void UpdateCacheFragmentation(double fragmentation_pct);

    // Profile Analysis & Generation
    [[nodiscard]] JitProfileSummary GenerateSummary() const;

    // Flame Graph Generator
    [[nodiscard]] std::string GenerateFlameGraphSvg() const;

    // Multi-format Exporters
    bool ExportToJson(const std::string& filepath) const;
    bool ExportToCsv(const std::string& filepath) const;
    bool ExportToHtml(const std::string& filepath) const;

private:
    void GenerateRecommendations(JitProfileSummary& summary) const;

    mutable std::mutex m_mutex;
    bool        m_active = false;
    std::string m_title_id;
    std::string m_module_name;

    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_end_time;

    std::unordered_map<uint64_t, BlockProfile> m_blocks;
    std::unordered_map<std::string, uint64_t>  m_opcode_counts;

    std::atomic<uint64_t> m_cache_hits{0};
    std::atomic<uint64_t> m_cache_misses{0};
    std::atomic<uint64_t> m_total_links{0};
    double m_cache_fragmentation = 0.0;

    TranslationPhaseMetrics m_total_phases{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_PROFILER_H
