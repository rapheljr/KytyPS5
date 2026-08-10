// MetalTaaPipelineTests.mm
//
// Unit & Integration Tests for Metal Temporal Anti-Aliasing (TAA) Pipeline.

#include "graphics/host_gpu/renderer/backend/metalTaaPipeline.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Graphics::HostGpu;

static void TestMetalTaaLifecycle() {
	std::printf("[TEST] MetalTaa_Lifecycle\n");

	MetalTaaPipeline taa;
	if (!taa.Initialize()) {
		std::fprintf(stderr, "FAIL: Initialize failed\n");
		std::exit(1);
	}

	if (!taa.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	taa.Shutdown();
	if (taa.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MetalTaa_Lifecycle\n");
}

static void TestHaltonJitterAndHistoryResolve() {
	std::printf("[TEST] MetalTaa_HaltonJitterAndResolve\n");

	MetalTaaPipeline taa;
	TaaConfig cfg{};
	cfg.blend_weight = 0.2f; // 20% current, 80% history
	taa.Initialize(cfg);

	// Test jitter generation
	auto j1 = taa.GetNextJitter();
	auto j2 = taa.GetNextJitter();

	if (j1.x == j2.x && j1.y == j2.y) {
		std::fprintf(stderr, "FAIL: Consecutive Halton jitter offsets are identical\n");
		std::exit(1);
	}

	// Test History Blending
	std::vector<float> curr = {1.0f, 1.0f, 1.0f, 1.0f};
	std::vector<float> hist = {0.0f, 0.0f, 0.0f, 1.0f};
	std::vector<float> out(4, 0.0f);

	taa.ResolveFrame(curr.data(), hist.data(), out.data(), 1);

	// Out should be 0.2 * 1.0 + 0.8 * 0.0 = 0.2
	if (std::abs(out[0] - 0.2f) > 1e-4 || std::abs(out[1] - 0.2f) > 1e-4 || std::abs(out[2] - 0.2f) > 1e-4) {
		std::fprintf(stderr, "FAIL: TAA blended color mismatch: R=%f, G=%f, B=%f\n", out[0], out[1], out[2]);
		std::exit(1);
	}

	taa.Shutdown();
	std::printf("  [ OK ] MetalTaa_HaltonJitterAndResolve\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal TAA Pipeline Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalTaaLifecycle();
	TestHaltonJitterAndHistoryResolve();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
