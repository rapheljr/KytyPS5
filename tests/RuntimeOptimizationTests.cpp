// RuntimeOptimizationTests.cpp
//
// Complete Test Suite for Modern Dynamic Runtime Optimization Layer.

#include "loader/recompiler/runtimeOptimizationEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "RuntimeOptimizationTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestHotBlockDetectionAndTieredPromotions() {
	std::printf("  [Opt Test 1] Testing Hot Block Detection & Tier Promotions...\n");

	RuntimeOptimizationEngine engine;
	uint64_t rip = 0x140001000ULL;
	ExecutionTier tier = ExecutionTier::Tier0_LazyFastJit;

	// Execute 99 times -> Tier 0
	for (int i = 0; i < 99; ++i) {
		bool promoted = engine.RecordExecution(rip, tier);
		Check(!promoted, "Must not promote before threshold");
		Check(tier == ExecutionTier::Tier0_LazyFastJit, "Must remain Tier 0");
	}

	// 100th execution -> Promotes to Tier 1
	bool promoted = engine.RecordExecution(rip, tier);
	Check(promoted, "100th execution must trigger promotion");
	Check(tier == ExecutionTier::Tier1_OptimizedJit, "Must promote to Tier 1");
	Check(engine.GetCounters().tier1_promotions.load() == 1, "Tier 1 promotion count mismatch");

	std::printf("  [OK] Opt Test 1: Hot Block Detection passed\n");
}

void TestInlineCachingAndCodeInvalidation() {
	std::printf("  [Opt Test 2] Testing Inline Cache & SMC Code Invalidation...\n");

	RuntimeOptimizationEngine engine;
	uint64_t call_site = 0x140002000ULL;
	uint64_t target_rip = 0x140003000ULL;
	uint64_t host_func  = 0x7FFF0000ULL;

	uint64_t cached_func = 0;
	Check(!engine.LookupInlineCache(call_site, target_rip, cached_func), "Cache must miss initially");

	engine.UpdateInlineCache(call_site, target_rip, host_func);

	Check(engine.LookupInlineCache(call_site, target_rip, cached_func), "Cache must hit after update");
	Check(cached_func == host_func, "Cached host function address mismatch");
	Check(engine.GetCounters().inline_cache_hits.load() == 1, "Cache hit counter mismatch");

	// SMC Code Invalidation over range [0x140003000, 0x140004000)
	engine.InvalidateCodeRange(0x140003000ULL, 0x1000);
	Check(!engine.LookupInlineCache(call_site, target_rip, cached_func), "Cache must miss after SMC invalidation");

	std::printf("  [OK] Opt Test 2: Inline Cache & SMC Invalidation passed\n");
}

void TestBackgroundRecompilerThreadPool() {
	std::printf("  [Opt Test 3] Testing Background Worker Thread Recompilation Queue...\n");

	RuntimeOptimizationEngine engine;
	std::atomic<bool> task_completed{false};

	engine.ScheduleBackgroundCompilation([&task_completed]() {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		task_completed = true;
	});

	// Wait for background worker thread to execute task
	for (int i = 0; i < 50; ++i) {
		if (task_completed.load()) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	Check(task_completed.load(), "Background task failed to execute");
	Check(engine.GetCounters().background_compilations_completed.load() == 1, "Completed task counter mismatch");

	std::printf("  [OK] Opt Test 3: Background Recompiler passed\n");
}

void TestOptimizationReportGeneration() {
	std::printf("  [Opt Test 4] Testing Optimization Statistics & Report Generation...\n");

	RuntimeOptimizationEngine engine;
	ExecutionTier tier = ExecutionTier::Tier0_LazyFastJit;
	engine.RecordExecution(0x140005000ULL, tier);
	engine.PromoteBlock(0x140005000ULL, ExecutionTier::Tier2_TraceJit);

	std::string report = engine.GenerateOptimizationReport();
	Check(!report.empty(), "Report must not be empty");
	Check(report.find("Runtime Optimization Engine Report") != std::string::npos, "Report header missing");
	Check(report.find("Tier 2 (Trace)") != std::string::npos, "Tier 2 entry missing in report");

	std::printf("  [OK] Opt Test 4: Optimization Report Generation passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Modern Runtime Optimization Engine Suite   \n");
	std::printf("====================================================\n");

	TestHotBlockDetectionAndTieredPromotions();
	TestInlineCachingAndCodeInvalidation();
	TestBackgroundRecompilerThreadPool();
	TestOptimizationReportGeneration();

	std::printf("\nALL RUNTIME OPTIMIZATION TESTS PASSED!\n");
	return 0;
}
