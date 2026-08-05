// jitProfiler.cpp
//
// JIT Profiler & Optimization Advisor Implementation.

#include "loader/recompiler/jitProfiler.h"

#include "common/logging/log.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Loader::Recompiler {

void JitProfiler::StartProfiling(const std::string& title_id, const std::string& module_name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_title_id    = title_id;
    m_module_name = module_name;
    m_active      = true;
    m_start_time  = std::chrono::steady_clock::now();

    m_blocks.clear();
    m_opcode_counts.clear();
    m_cache_hits.store(0);
    m_cache_misses.store(0);
    m_total_links.store(0);
    m_cache_fragmentation = 0.0;
    m_total_phases        = TranslationPhaseMetrics{};
}

void JitProfiler::StopProfiling() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_active   = false;
    m_end_time = std::chrono::steady_clock::now();
}

void JitProfiler::RecordBlockTranslation(uint64_t guest_rip, size_t size_bytes,
                                           const TranslationPhaseMetrics& phases,
                                           uint32_t ir_cnt, uint32_t arm64_cnt,
                                           uint32_t simd_cnt, uint32_t branch_cnt,
                                           uint32_t mem_cnt) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto& block = m_blocks[guest_rip];
    block.guest_rip             = guest_rip;
    block.size_bytes            = size_bytes;
    block.translation_time_ns   += phases.total_time_ns;
    block.ir_instruction_cnt    = ir_cnt;
    block.arm64_instruction_cnt = arm64_cnt;
    block.simd_instruction_cnt  = simd_cnt;
    block.branch_instruction_cnt= branch_cnt;
    block.memory_inst_count     = mem_cnt;

    m_total_phases.lowering_time_ns += phases.lowering_time_ns;
    m_total_phases.ir_opt_time_ns   += phases.ir_opt_time_ns;
    m_total_phases.reg_alloc_time_ns+= phases.reg_alloc_time_ns;
    m_total_phases.codegen_time_ns  += phases.codegen_time_ns;
    m_total_phases.total_time_ns    += phases.total_time_ns;
}

void JitProfiler::RecordBlockExecution(uint64_t guest_rip, uint64_t exec_time_ns,
                                         uint64_t stall_cycles, uint64_t mispredicts) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_blocks.find(guest_rip);
    if (it != m_blocks.end()) {
        it->second.execution_count++;
        it->second.total_exec_time_ns += exec_time_ns;
        it->second.memory_stall_cycles += stall_cycles;
        it->second.branch_mispredicts   += mispredicts;
    } else {
        BlockProfile b;
        b.guest_rip            = guest_rip;
        b.execution_count       = 1;
        b.total_exec_time_ns    = exec_time_ns;
        b.memory_stall_cycles  = stall_cycles;
        b.branch_mispredicts   = mispredicts;
        m_blocks[guest_rip]     = b;
    }
}

void JitProfiler::RecordCacheHit() {
    m_cache_hits.fetch_add(1, std::memory_order_relaxed);
}

void JitProfiler::RecordCacheMiss() {
    m_cache_misses.fetch_add(1, std::memory_order_relaxed);
}

void JitProfiler::RecordBlockLinked(uint64_t src_rip, uint64_t /*dst_rip*/) {
    m_total_links.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_blocks.find(src_rip);
    if (it != m_blocks.end()) {
        it->second.is_linked = true;
    }
}

void JitProfiler::RecordOpcode(const std::string& opcode_name, uint64_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_opcode_counts[opcode_name] += count;
}

void JitProfiler::UpdateCacheFragmentation(double fragmentation_pct) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache_fragmentation = fragmentation_pct;
}

JitProfileSummary JitProfiler::GenerateSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    JitProfileSummary s;
    s.title_id     = m_title_id.empty() ? "UNKNOWN" : m_title_id;
    s.module_name  = m_module_name.empty() ? "eboot.bin" : m_module_name;

    s.total_unique_blocks = m_blocks.size();
    s.phase_breakdown     = m_total_phases;
    s.cache_fragmentation_pct = m_cache_fragmentation;

    uint64_t total_hits   = m_cache_hits.load();
    uint64_t total_misses = m_cache_misses.load();
    uint64_t total_dispatches = total_hits + total_misses;

    s.cache_hit_rate_pct = total_dispatches > 0
        ? (100.0 * static_cast<double>(total_hits) / static_cast<double>(total_dispatches))
        : 0.0;

    uint64_t total_arm64_insts = 0;
    uint64_t total_simd_insts  = 0;
    uint64_t total_branches    = 0;
    uint64_t total_mispredicts  = 0;

    double sum_exec_ns = 0.0;
    double max_exec_ns = 0.0;

    std::vector<BlockProfile> all_blocks;
    all_blocks.reserve(m_blocks.size());

    for (const auto& [rip, b] : m_blocks) {
        all_blocks.push_back(b);
        s.total_executions += b.execution_count;

        if (b.execution_count >= 10) {
            s.hot_block_count++;
        } else if (b.execution_count == 1) {
            s.cold_block_count++;
        }

        total_arm64_insts += b.arm64_instruction_cnt * b.execution_count;
        total_simd_insts  += b.simd_instruction_cnt * b.execution_count;
        total_branches    += b.branch_instruction_cnt * b.execution_count;
        total_mispredicts += b.branch_mispredicts;
        s.total_memory_stalls += b.memory_stall_cycles;

        double block_avg_ns = b.execution_count > 0
            ? static_cast<double>(b.total_exec_time_ns) / static_cast<double>(b.execution_count)
            : 0.0;

        sum_exec_ns += b.total_exec_time_ns;
        max_exec_ns = std::max(max_exec_ns, block_avg_ns);
    }

    s.block_reuse_ratio = s.total_unique_blocks > 0
        ? static_cast<double>(s.total_executions) / static_cast<double>(s.total_unique_blocks)
        : 0.0;

    s.avg_exec_latency_ns  = s.total_executions > 0 ? (sum_exec_ns / s.total_executions) : 0.0;
    s.max_exec_latency_ns  = max_exec_ns;
    s.avg_trans_latency_ns = s.total_unique_blocks > 0
        ? (static_cast<double>(m_total_phases.total_time_ns) / s.total_unique_blocks)
        : 0.0;

    s.simd_usage_pct = total_arm64_insts > 0
        ? (100.0 * static_cast<double>(total_simd_insts) / static_cast<double>(total_arm64_insts))
        : 0.0;

    s.branch_prediction_pct = total_branches > 0
        ? std::max(0.0, 100.0 - (100.0 * static_cast<double>(total_mispredicts) / static_cast<double>(total_branches)))
        : 100.0;

    // Sort hot blocks by execution count (descending)
    std::sort(all_blocks.begin(), all_blocks.end(),
              [](const BlockProfile& a, const BlockProfile& b) {
                  return a.execution_count > b.execution_count;
              });

    if (all_blocks.size() > 20) {
        s.hot_blocks.assign(all_blocks.begin(), all_blocks.begin() + 20);
    } else {
        s.hot_blocks = all_blocks;
    }

    // Identify cold blocks (executed == 1)
    for (const auto& b : all_blocks) {
        if (b.execution_count == 1) {
            s.cold_blocks.push_back(b);
            if (s.cold_blocks.size() >= 20) break;
        }
    }

    // Top opcodes
    std::vector<std::pair<std::string, uint64_t>> opc_vec(m_opcode_counts.begin(), m_opcode_counts.end());
    std::sort(opc_vec.begin(), opc_vec.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });
    if (opc_vec.size() > 20) opc_vec.resize(20);
    s.top_opcodes = std::move(opc_vec);

    GenerateRecommendations(s);
    return s;
}

void JitProfiler::GenerateRecommendations(JitProfileSummary& summary) const {
    std::vector<OptimizationRecommendation> recs;
    uint32_t rank = 1;

    // Recommendation 1: Unvectorized SIMD Math in Hot Blocks
    for (const auto& b : summary.hot_blocks) {
        if (b.execution_count >= 100 && b.simd_instruction_cnt == 0 && b.arm64_instruction_cnt > 10) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "0x%" PRIx64, b.guest_rip);

            OptimizationRecommendation r;
            r.rank = rank++;
            r.category = "SIMD Vectorization";
            r.title = "Vectorize Hot Block " + std::string(buf);
            r.description = "Block executed " + std::to_string(b.execution_count) +
                            " times with 0 SIMD vector instructions. Lowering to ARM64 NEON registers can yield up to 4x throughput.";
            r.expected_speedup_x = 1.45;
            r.target_rip_hex = buf;
            recs.push_back(r);
            break; // Recommend top candidate
        }
    }

    // Recommendation 2: Unlinked Hot Block Branches
    for (const auto& b : summary.hot_blocks) {
        if (b.execution_count >= 50 && !b.is_linked && b.branch_instruction_cnt > 0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "0x%" PRIx64, b.guest_rip);

            OptimizationRecommendation r;
            r.rank = rank++;
            r.category = "Block Linking";
            r.title = "Direct Block Link for " + std::string(buf);
            r.description = "Hot branch target at " + std::string(buf) + " dispatches through central runtime bridge. Link block directly in JIT code cache.";
            r.expected_speedup_x = 1.28;
            r.target_rip_hex = buf;
            recs.push_back(r);
            break;
        }
    }

    // Recommendation 3: High Cold Block Memory Pressure
    if (summary.cold_block_count > 50 && summary.cold_block_count > summary.hot_block_count * 2) {
        OptimizationRecommendation r;
        r.rank = rank++;
        r.category = "Cache Eviction";
        r.title = "Enable Generational LRU Cache Compaction";
        r.description = "Detected " + std::to_string(summary.cold_block_count) +
                        " single-use cold blocks polluting Gen0 code cache. Enabling LRU compaction frees executable memory and reduces TLB misses.";
        r.expected_speedup_x = 1.15;
        r.target_rip_hex = "N/A";
        recs.push_back(r);
    }

    // Recommendation 4: Register Allocation / Memory Stalls
    if (summary.total_memory_stalls > 1000) {
        OptimizationRecommendation r;
        r.rank = rank++;
        r.category = "Register Allocation";
        r.title = "Spill Minimization in Hot Loops";
        r.description = "High memory stall count (" + std::to_string(summary.total_memory_stalls) +
                        " cycles) detected. Apply linear-scan live-range splitting to keep hot variables in ARM64 registers X19-X28.";
        r.expected_speedup_x = 1.22;
        r.target_rip_hex = "Global";
        recs.push_back(r);
    }

    // Recommendation 5: High IR Optimization Overhead
    if (summary.phase_breakdown.ir_opt_time_ns > summary.phase_breakdown.lowering_time_ns * 2) {
        OptimizationRecommendation r;
        r.rank = rank++;
        r.category = "Tiered Compilation";
        r.title = "Implement Tiered JIT Compilation (Tier 0 Interpreter + Tier 1 JIT)";
        r.description = "IR optimization pass overhead is high (" +
                        std::to_string(summary.phase_breakdown.ir_opt_time_ns / 1000) +
                        " us). Skip heavy passes (Dominator tree / RLE) for Tier 0 initial execution.";
        r.expected_speedup_x = 1.30;
        r.target_rip_hex = "Pipeline";
        recs.push_back(r);
    }

    summary.recommendations = std::move(recs);
}

std::string JitProfiler::GenerateFlameGraphSvg() const {
    const auto s = GenerateSummary();

    std::ostringstream svg;
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"800\" height=\"320\" viewBox=\"0 0 800 320\">\n"
        << "  <style>\n"
        << "    .flame-title { font-family: system-ui, sans-serif; font-size: 14px; font-weight: bold; fill: #ffffff; }\n"
        << "    .flame-rect  { stroke: #12131c; stroke-width: 1px; rx: 3px; ry: 3px; transition: opacity 0.2s; }\n"
        << "    .flame-rect:hover { opacity: 0.85; cursor: pointer; }\n"
        << "    .flame-text  { font-family: monospace; font-size: 11px; fill: #ffffff; text-anchor: middle; dominant-baseline: central; }\n"
        << "  </style>\n"
        << "  <rect width=\"100%\" height=\"100%\" fill=\"#12131c\" rx=\"8\" />\n"
        << "  <text x=\"20\" y=\"25\" class=\"flame-title\">ARM64 JIT Pipeline &amp; Execution Flame Graph — "
        << s.title_id << "</text>\n";

    // Row 1: Translation Phases (y=50, h=35)
    uint64_t total_trans_ns = s.phase_breakdown.total_time_ns > 0 ? s.phase_breakdown.total_time_ns : 1;
    double w_lowering = 760.0 * (static_cast<double>(s.phase_breakdown.lowering_time_ns) / total_trans_ns);
    double w_iropt    = 760.0 * (static_cast<double>(s.phase_breakdown.ir_opt_time_ns) / total_trans_ns);
    double w_regalloc = 760.0 * (static_cast<double>(s.phase_breakdown.reg_alloc_time_ns) / total_trans_ns);
    double w_codegen  = 760.0 * (static_cast<double>(s.phase_breakdown.codegen_time_ns) / total_trans_ns);

    double x_curr = 20.0;
    if (w_lowering > 1.0) {
        svg << "  <rect x=\"" << x_curr << "\" y=\"50\" width=\"" << w_lowering << "\" height=\"35\" fill=\"#ff5252\" class=\"flame-rect\" />\n"
            << "  <text x=\"" << (x_curr + w_lowering / 2.0) << "\" y=\"67\" class=\"flame-text\">Lowering</text>\n";
        x_curr += w_lowering;
    }
    if (w_iropt > 1.0) {
        svg << "  <rect x=\"" << x_curr << "\" y=\"50\" width=\"" << w_iropt << "\" height=\"35\" fill=\"#ff9100\" class=\"flame-rect\" />\n"
            << "  <text x=\"" << (x_curr + w_iropt / 2.0) << "\" y=\"67\" class=\"flame-text\">IR Opt</text>\n";
        x_curr += w_iropt;
    }
    if (w_regalloc > 1.0) {
        svg << "  <rect x=\"" << x_curr << "\" y=\"50\" width=\"" << w_regalloc << "\" height=\"35\" fill=\"#ffd600\" class=\"flame-rect\" />\n"
            << "  <text x=\"" << (x_curr + w_regalloc / 2.0) << "\" y=\"67\" class=\"flame-text\">RegAlloc</text>\n";
        x_curr += w_regalloc;
    }
    if (w_codegen > 1.0) {
        svg << "  <rect x=\"" << x_curr << "\" y=\"50\" width=\"" << w_codegen << "\" height=\"35\" fill=\"#00e676\" class=\"flame-rect\" />\n"
            << "  <text x=\"" << (x_curr + w_codegen / 2.0) << "\" y=\"67\" class=\"flame-text\">Codegen</text>\n";
    }

    // Row 2: Top Hot Blocks Execution Hierarchy (y=100..270)
    svg << "  <text x=\"20\" y=\"110\" class=\"flame-title\">Hot Block Execution Towers (Guest RIP &amp; Hotness)</text>\n";

    x_curr = 20.0;
    uint64_t total_execs = s.total_executions > 0 ? s.total_executions : 1;

    static const char* kHotColors[] = { "#00b0ff", "#7c4dff", "#ff4081", "#651fff", "#00e5ff" };
    size_t color_idx = 0;

    for (const auto& b : s.hot_blocks) {
        double block_w = 760.0 * (static_cast<double>(b.execution_count) / total_execs);
        if (block_w < 15.0) block_w = 15.0; // Min width for visibility

        if (x_curr + block_w > 780.0) break;

        char rip_buf[32];
        std::snprintf(rip_buf, sizeof(rip_buf), "0x%" PRIx64, b.guest_rip);

        const char* col = kHotColors[color_idx++ % 5];
        svg << "  <rect x=\"" << x_curr << "\" y=\"130\" width=\"" << (block_w - 2.0)
            << "\" height=\"140\" fill=\"" << col << "\" class=\"flame-rect\" />\n"
            << "  <text x=\"" << (x_curr + (block_w - 2.0) / 2.0) << "\" y=\"200\" class=\"flame-text\" transform=\"rotate(-90 "
            << (x_curr + (block_w - 2.0) / 2.0) << " 200)\">" << rip_buf << " (" << b.execution_count << "x)</text>\n";

        x_curr += block_w;
    }

    svg << "</svg>\n";
    return svg.str();
}

bool JitProfiler::ExportToJson(const std::string& filepath) const {
    const auto s = GenerateSummary();
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "{\n"
      << "  \"title_id\": \"" << s.title_id << "\",\n"
      << "  \"module_name\": \"" << s.module_name << "\",\n"
      << "  \"metrics\": {\n"
      << "    \"total_unique_blocks\": " << s.total_unique_blocks << ",\n"
      << "    \"hot_block_count\": " << s.hot_block_count << ",\n"
      << "    \"cold_block_count\": " << s.cold_block_count << ",\n"
      << "    \"total_executions\": " << s.total_executions << ",\n"
      << "    \"block_reuse_ratio\": " << s.block_reuse_ratio << ",\n"
      << "    \"cache_hit_rate_pct\": " << s.cache_hit_rate_pct << ",\n"
      << "    \"cache_fragmentation_pct\": " << s.cache_fragmentation_pct << ",\n"
      << "    \"simd_usage_pct\": " << s.simd_usage_pct << ",\n"
      << "    \"branch_prediction_pct\": " << s.branch_prediction_pct << ",\n"
      << "    \"avg_exec_latency_ns\": " << s.avg_exec_latency_ns << ",\n"
      << "    \"avg_trans_latency_ns\": " << s.avg_trans_latency_ns << "\n"
      << "  },\n"
      << "  \"phase_breakdown_ns\": {\n"
      << "    \"lowering\": " << s.phase_breakdown.lowering_time_ns << ",\n"
      << "    \"ir_opt\": " << s.phase_breakdown.ir_opt_time_ns << ",\n"
      << "    \"reg_alloc\": " << s.phase_breakdown.reg_alloc_time_ns << ",\n"
      << "    \"codegen\": " << s.phase_breakdown.codegen_time_ns << "\n"
      << "  },\n"
      << "  \"hot_blocks\": [\n";

    for (size_t i = 0; i < s.hot_blocks.size(); ++i) {
        const auto& b = s.hot_blocks[i];
        f << "    {\n"
          << "      \"guest_rip\": \"0x" << std::hex << b.guest_rip << std::dec << "\",\n"
          << "      \"execution_count\": " << b.execution_count << ",\n"
          << "      \"total_exec_time_ns\": " << b.total_exec_time_ns << ",\n"
          << "      \"simd_instruction_cnt\": " << b.simd_instruction_cnt << ",\n"
          << "      \"is_linked\": " << (b.is_linked ? "true" : "false") << "\n"
          << "    }" << (i + 1 < s.hot_blocks.size() ? "," : "") << "\n";
    }
    f << "  ],\n";

    f << "  \"recommendations\": [\n";
    for (size_t i = 0; i < s.recommendations.size(); ++i) {
        const auto& r = s.recommendations[i];
        f << "    {\n"
          << "      \"rank\": " << r.rank << ",\n"
          << "      \"category\": \"" << r.category << "\",\n"
          << "      \"title\": \"" << r.title << "\",\n"
          << "      \"expected_speedup_x\": " << r.expected_speedup_x << ",\n"
          << "      \"target_rip\": \"" << r.target_rip_hex << "\"\n"
          << "    }" << (i + 1 < s.recommendations.size() ? "," : "") << "\n";
    }
    f << "  ]\n";
    f << "}\n";
    return true;
}

bool JitProfiler::ExportToCsv(const std::string& filepath) const {
    const auto s = GenerateSummary();
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "GuestRIP,ExecutionCount,TotalExecTimeNs,SIMDInstCount,BranchInstCount,IsLinked\n";
    for (const auto& b : s.hot_blocks) {
        f << "0x" << std::hex << b.guest_rip << std::dec << ","
          << b.execution_count << ","
          << b.total_exec_time_ns << ","
          << b.simd_instruction_cnt << ","
          << b.branch_instruction_cnt << ","
          << (b.is_linked ? "1" : "0") << "\n";
    }
    return true;
}

bool JitProfiler::ExportToHtml(const std::string& filepath) const {
    const auto s = GenerateSummary();
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
      << "<meta charset=\"UTF-8\">\n<title>KytyPS5 JIT Profiler Report — " << s.title_id << "</title>\n"
      << "<style>\n"
      << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0b0c10; color: #c5c6c7; margin: 0; padding: 24px; }\n"
      << "  h1, h2 { color: #66fcf1; }\n"
      << "  .card-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 16px; margin-bottom: 24px; }\n"
      << "  .card { background: #1f2833; padding: 16px; border-radius: 8px; border: 1px solid #45a29e; }\n"
      << "  .card-val { font-size: 24px; font-weight: bold; color: #45a29e; margin-top: 8px; }\n"
      << "  table { width: 100%; border-collapse: collapse; margin-top: 12px; background: #1f2833; border-radius: 8px; overflow: hidden; }\n"
      << "  th, td { padding: 12px; text-align: left; border-bottom: 1px solid #0b0c10; }\n"
      << "  th { background: #45a29e; color: #0b0c10; }\n"
      << "  .rec-card { background: #1f2833; border-left: 4px solid #66fcf1; padding: 16px; margin-bottom: 12px; border-radius: 4px; }\n"
      << "  .badge { background: #66fcf1; color: #0b0c10; font-weight: bold; padding: 4px 8px; border-radius: 4px; display: inline-block; }\n"
      << "</style>\n</head>\n<body>\n";

    f << "<h1>KytyPS5 JIT Profiler Dashboard</h1>\n"
      << "<p>Title ID: <strong>" << s.title_id << "</strong> | Module: <strong>" << s.module_name << "</strong></p>\n";

    // Summary Cards
    f << "<div class=\"card-grid\">\n"
      << "  <div class=\"card\"><div>Unique Blocks</div><div class=\"card-val\">" << s.total_unique_blocks << "</div></div>\n"
      << "  <div class=\"card\"><div>Block Reuse Ratio</div><div class=\"card-val\">" << std::fixed << std::setprecision(1) << s.block_reuse_ratio << "x</div></div>\n"
      << "  <div class=\"card\"><div>Cache Hit Rate</div><div class=\"card-val\">" << s.cache_hit_rate_pct << "%</div></div>\n"
      << "  <div class=\"card\"><div>SIMD Vector Usage</div><div class=\"card-val\">" << s.simd_usage_pct << "%</div></div>\n"
      << "  <div class=\"card\"><div>Branch Accuracy</div><div class=\"card-val\">" << s.branch_prediction_pct << "%</div></div>\n"
      << "</div>\n";

    // Flame Graph SVG
    f << "<h2>JIT Flame Graph</h2>\n"
      << GenerateFlameGraphSvg() << "\n";

    // Optimization Advisor
    f << "<h2>Ranked Optimization Opportunities</h2>\n";
    for (const auto& r : s.recommendations) {
        f << "<div class=\"rec-card\">\n"
          << "  <span class=\"badge\">Rank #" << r.rank << " &mdash; " << r.expected_speedup_x << "x Speedup</span>\n"
          << "  <h3 style=\"margin: 8px 0 4px 0; color: #66fcf1;\">[" << r.category << "] " << r.title << "</h3>\n"
          << "  <p style=\"margin: 0; color: #c5c6c7;\">" << r.description << "</p>\n"
          << "</div>\n";
    }

    // Hot Blocks Table
    f << "<h2>Top Hot Blocks</h2>\n<table>\n"
      << "<tr><th>Guest RIP</th><th>Exec Count</th><th>Total Exec (ns)</th><th>SIMD Insts</th><th>Branch Insts</th><th>Linked</th></tr>\n";

    for (const auto& b : s.hot_blocks) {
        f << "<tr>"
          << "<td>0x" << std::hex << b.guest_rip << std::dec << "</td>"
          << "<td>" << b.execution_count << "</td>"
          << "<td>" << b.total_exec_time_ns << "</td>"
          << "<td>" << b.simd_instruction_cnt << "</td>"
          << "<td>" << b.branch_instruction_cnt << "</td>"
          << "<td>" << (b.is_linked ? "<span style=\"color:#00e676;\">Yes</span>" : "<span style=\"color:#ff5252;\">No</span>") << "</td>"
          << "</tr>\n";
    }
    f << "</table>\n</body>\n</html>\n";
    return true;
}

} // namespace Loader::Recompiler
