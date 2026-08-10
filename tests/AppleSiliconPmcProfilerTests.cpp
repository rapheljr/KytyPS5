// AppleSiliconPmcProfilerTests.cpp
//
// Unit & Integration Tests for Apple Silicon Hardware PMC Profiler.

#include "loader/recompiler/appleSiliconPmcProfiler.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static void TestPmcSessionLifecycle() {
	std::printf("[TEST] AppleSiliconPmc_Lifecycle\n");

	AppleSiliconPmcProfiler profiler;
	if (!profiler.StartSession()) {
		std::fprintf(stderr, "FAIL: StartSession failed\n");
		std::exit(1);
	}

	if (!profiler.IsActive()) {
		std::fprintf(stderr, "FAIL: IsActive returned false during session\n");
		std::exit(1);
	}

	profiler.StopSession();
	if (profiler.IsActive()) {
		std::fprintf(stderr, "FAIL: IsActive returned true after stop\n");
		std::exit(1);
	}

	std::printf("  [ OK ] AppleSiliconPmc_Lifecycle\n");
}

static void TestPmcSampleCalculations() {
	std::printf("[TEST] AppleSiliconPmc_SampleCalculations\n");

	AppleSiliconPmcProfiler profiler;
	profiler.StartSession();

	// 1,000,000 instrs in 500,000 cycles -> IPC = 2.0
	// 5,000 branch misses -> branch miss rate = 0.5%
	// 1,000 L1I cache misses
	profiler.RecordSample(1000000, 500000, 5000, 1000);

	auto snap = profiler.StopSession();

	if (std::abs(snap.ipc - 2.0) > 1e-4) {
		std::fprintf(stderr, "FAIL: Expected IPC 2.0, got %f\n", snap.ipc);
		std::exit(1);
	}

	if (std::abs(snap.branch_miss_rate_pct - 0.5) > 1e-4) {
		std::fprintf(stderr, "FAIL: Expected branch miss rate 0.5%%, got %f\n", snap.branch_miss_rate_pct);
		std::exit(1);
	}

	if (snap.l1i_cache_misses != 1000) {
		std::fprintf(stderr, "FAIL: L1I cache misses mismatch: %llu\n", snap.l1i_cache_misses);
		std::exit(1);
	}

	std::printf("  [ OK ] AppleSiliconPmc_SampleCalculations\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Apple Silicon Hardware PMC Profiler Test Suite\n");
	std::printf("================================================================================\n");

	TestPmcSessionLifecycle();
	TestPmcSampleCalculations();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
