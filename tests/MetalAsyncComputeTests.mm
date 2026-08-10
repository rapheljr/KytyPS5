// MetalAsyncComputeTests.mm
//
// Unit & Integration Tests for Metal Dedicated Asynchronous Compute & Multi-Queue Synchronization.

#include "graphics/host_gpu/renderer/backend/metalAsyncCompute.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Graphics::HostGpu::Metal;

void TestMetalAsyncComputeLifecycle() {
	std::printf("[TEST] MetalAsyncCompute_Lifecycle\n");

	MetalAsyncComputeEngine engine;
	bool ok = engine.Initialize();
	Check(ok, "MetalAsyncComputeEngine Initialize failed");
	Check(engine.IsInitialized(), "Engine should be initialized");

	engine.Shutdown();
	Check(!engine.IsInitialized(), "Engine should be shutdown");

	std::printf("  [ OK ] MetalAsyncCompute_Lifecycle\n");
}

void TestMetalAsyncComputeDispatchAndSync() {
	std::printf("[TEST] MetalAsyncCompute_DispatchAndSync\n");

	MetalAsyncComputeEngine engine;
	bool ok = engine.Initialize();
	Check(ok, "MetalAsyncComputeEngine Initialize failed");

	// Dispatch 5 asynchronous compute workloads
	for (int i = 0; i < 5; ++i) {
		bool disp_ok = engine.DispatchCompute(16, 16, 1, 8, 8, 1);
		Check(disp_ok, "DispatchCompute failed");
	}

	Check(engine.GetStats().compute_dispatches_total == 5, "Expected 5 compute dispatches");

	// Insert cross-queue synchronization barrier
	bool bar_ok = engine.InsertCrossQueueBarrier();
	Check(bar_ok, "InsertCrossQueueBarrier failed");
	Check(engine.GetStats().sync_barriers_total == 1, "Expected 1 sync barrier");

	// Wait for completion
	engine.WaitForIdle();

	engine.Shutdown();
	std::printf("  [ OK ] MetalAsyncCompute_DispatchAndSync\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal Dedicated Asynchronous Compute Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalAsyncComputeLifecycle();
	TestMetalAsyncComputeDispatchAndSync();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
