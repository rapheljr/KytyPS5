// JitProfilerTests.cpp
//
// Integration & Unit Test Suite for KytyPS5 Full JIT Profiler & Optimization Advisor.

#include "loader/recompiler/jitProfiler.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using Loader::Recompiler::JitProfiler;
using Loader::Recompiler::TranslationPhaseMetrics;

#define TEST(name) static void test_##name(); static const bool reg_##name = (test_##name(), true); static void test_##name()
#define ASSERT_TRUE(cond) assert(cond)
#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_GT(a, b) assert((a) > (b))

TEST(JitProfilerSessionMetrics) {
    JitProfiler profiler;
    profiler.StartProfiling("TEST_APP", "eboot.bin");

    TranslationPhaseMetrics phases;
    phases.lowering_time_ns  = 1000;
    phases.ir_opt_time_ns    = 2000;
    phases.reg_alloc_time_ns = 500;
    phases.codegen_time_ns   = 1500;
    phases.total_time_ns     = 5000;

    // Record translation for 2 blocks
    profiler.RecordBlockTranslation(0x400000, 64, phases, 8, 16, 4, 1, 3);
    profiler.RecordBlockTranslation(0x400100, 128, phases, 12, 24, 0, 2, 4);

    // Record executions
    for (int i = 0; i < 150; ++i) {
        profiler.RecordBlockExecution(0x400000, 120, 2, 0); // Hot block
        profiler.RecordCacheHit();
    }
    for (int i = 0; i < 5; ++i) {
        profiler.RecordBlockExecution(0x400100, 300, 10, 1);
        profiler.RecordCacheHit();
    }
    profiler.RecordCacheMiss();
    profiler.RecordCacheMiss();

    profiler.RecordOpcode("VADD.4S", 150);
    profiler.RecordOpcode("MOV", 300);

    profiler.StopProfiling();

    auto summary = profiler.GenerateSummary();

    ASSERT_EQ(summary.title_id, "TEST_APP");
    ASSERT_EQ(summary.total_unique_blocks, static_cast<size_t>(2));
    ASSERT_EQ(summary.total_executions, static_cast<uint64_t>(155));
    ASSERT_GT(summary.cache_hit_rate_pct, 95.0); // 155 hits / 157 total dispatches
    ASSERT_GT(summary.simd_usage_pct, 0.0);
    ASSERT_GT(summary.hot_blocks.size(), static_cast<size_t>(0));
    ASSERT_EQ(summary.hot_blocks[0].guest_rip, static_cast<uint64_t>(0x400000));

    std::printf("  [OK] JitProfilerSessionMetrics: hits=%.1f%%, reuse=%.1fx, SIMD=%.1f%%\n",
                summary.cache_hit_rate_pct, summary.block_reuse_ratio, summary.simd_usage_pct);
}

TEST(JitProfilerFlameGraphSvg) {
    JitProfiler profiler;
    profiler.StartProfiling("FLAME_TEST", "eboot.bin");

    TranslationPhaseMetrics phases;
    phases.lowering_time_ns  = 5000;
    phases.ir_opt_time_ns    = 10000;
    phases.reg_alloc_time_ns = 3000;
    phases.codegen_time_ns   = 7000;
    phases.total_time_ns     = 25000;

    profiler.RecordBlockTranslation(0x500000, 128, phases, 20, 30, 10, 2, 5);
    profiler.RecordBlockExecution(0x500000, 500);

    const std::string svg = profiler.GenerateFlameGraphSvg();

    ASSERT_TRUE(svg.find("<svg") != std::string::npos);
    ASSERT_TRUE(svg.find("Lowering") != std::string::npos);
    ASSERT_TRUE(svg.find("IR Opt") != std::string::npos);
    ASSERT_TRUE(svg.find("0x500000") != std::string::npos);

    std::printf("  [OK] JitProfilerFlameGraphSvg: rendered %zu bytes of SVG\n", svg.size());
}

TEST(JitProfilerExportersAndAdvisor) {
    JitProfiler profiler;
    profiler.StartProfiling("EXPORTER_TEST", "main.elf");

    TranslationPhaseMetrics phases;
    phases.lowering_time_ns  = 2000;
    phases.ir_opt_time_ns    = 8000; // High IR opt time
    phases.reg_alloc_time_ns = 1000;
    phases.codegen_time_ns   = 3000;
    phases.total_time_ns     = 14000;

    // Record hot unvectorized block
    profiler.RecordBlockTranslation(0x600000, 256, phases, 30, 50, 0 /* 0 SIMD */, 4, 10);
    for (int i = 0; i < 200; ++i) {
        profiler.RecordBlockExecution(0x600000, 1500, 15 /* stall */, 2);
    }

    const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "kyty_profiler_test";
    std::filesystem::create_directories(tmp_dir);

    const std::string json_path = (tmp_dir / "profile.json").string();
    const std::string csv_path  = (tmp_dir / "profile.csv").string();
    const std::string html_path = (tmp_dir / "profile.html").string();

    ASSERT_TRUE(profiler.ExportToJson(json_path));
    ASSERT_TRUE(profiler.ExportToCsv(csv_path));
    ASSERT_TRUE(profiler.ExportToHtml(html_path));

    ASSERT_TRUE(std::filesystem::exists(json_path));
    ASSERT_TRUE(std::filesystem::exists(csv_path));
    ASSERT_TRUE(std::filesystem::exists(html_path));

    auto summary = profiler.GenerateSummary();
    ASSERT_GT(summary.recommendations.size(), static_cast<size_t>(0));

    std::printf("  Top recommendation (#1): %s — %s (%.2fx expected speedup)\n",
                summary.recommendations[0].category.c_str(),
                summary.recommendations[0].title.c_str(),
                summary.recommendations[0].expected_speedup_x);

    std::printf("  [OK] JitProfilerExportersAndAdvisor: exports created successfully\n");

    std::filesystem::remove_all(tmp_dir);
}

int main() {
    std::printf("\n====================================================\n");
    std::printf(" KytyPS5: Full JIT Profiler Integration Test Suite  \n");
    std::printf("====================================================\n\n");

    std::printf("All 3 JIT Profiler integration tests passed successfully!\n\n");
    return 0;
}
