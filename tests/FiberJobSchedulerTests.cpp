// FiberJobSchedulerTests.cpp
//
// Unit & Integration Tests for PS5 Fiber Job Scheduler and Dispatch Queue.

#include "kernel/ps5FiberScheduler.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>

using namespace Kernel;

static void TestFiberSchedulerLifecycle() {
	std::printf("[TEST] FiberScheduler_Lifecycle\n");

	Ps5FiberScheduler scheduler(4);
	if (!scheduler.Initialize()) {
		std::fprintf(stderr, "FAIL: Ps5FiberScheduler initialization failed\n");
		std::exit(1);
	}

	if (!scheduler.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	scheduler.Shutdown();
	if (scheduler.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] FiberScheduler_Lifecycle\n");
}

static void TestJobDispatchAndExecution() {
	std::printf("[TEST] FiberScheduler_JobDispatchAndExecution\n");

	Ps5FiberScheduler scheduler(4);
	scheduler.Initialize();

	std::atomic<uint64_t> counter{0};

	// Submit 10 jobs
	for (int i = 0; i < 10; ++i) {
		scheduler.SubmitJob([&counter](void* /*ud*/) {
			counter.fetch_add(1);
		});
	}

	scheduler.WaitForAll();

	if (counter.load() != 10) {
		std::fprintf(stderr, "FAIL: Expected counter=10, got %llu\n", counter.load());
		std::exit(1);
	}

	const auto& stats = scheduler.GetStats();
	if (stats.total_jobs_dispatched != 10 || stats.total_jobs_completed != 10) {
		std::fprintf(stderr, "FAIL: Stats mismatch (Dispatched=%llu, Completed=%llu)\n",
		             stats.total_jobs_dispatched, stats.total_jobs_completed);
		std::exit(1);
	}

	scheduler.Shutdown();
	std::printf("  [ OK ] FiberScheduler_JobDispatchAndExecution\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 Fiber Job Dispatch Scheduler Test Suite\n");
	std::printf("================================================================================\n");

	TestFiberSchedulerLifecycle();
	TestJobDispatchAndExecution();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
