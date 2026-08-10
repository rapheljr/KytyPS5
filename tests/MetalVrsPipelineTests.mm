// MetalVrsPipelineTests.mm
//
// Unit & Integration Tests for Metal Variable Rate Shading & Dynamic Resolution Scaling.

#include "graphics/host_gpu/renderer/backend/metalVrsPipeline.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Graphics::HostGpu;

static void TestMetalVrsLifecycle() {
	std::printf("[TEST] MetalVrs_Lifecycle\n");

	MetalVrsPipeline vrs;
	if (!vrs.Initialize()) {
		std::fprintf(stderr, "FAIL: Initialize failed\n");
		std::exit(1);
	}

	if (!vrs.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	vrs.Shutdown();
	if (vrs.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MetalVrs_Lifecycle\n");
}

static void TestDrsAndVrsDynamicAdjustment() {
	std::printf("[TEST] MetalVrs_DynamicAdjustment\n");

	MetalVrsPipeline vrs;
	VrsDrsConfig cfg{};
	cfg.target_frame_time_ms = 16.666f; // 60 FPS
	cfg.min_render_scale     = 0.5f;
	cfg.max_render_scale     = 1.0f;
	vrs.Initialize(cfg);

	// Initial render scale should be max
	if (vrs.GetCurrentRenderScale() != 1.0f) {
		std::fprintf(stderr, "FAIL: Initial render scale not 1.0\n");
		std::exit(1);
	}

	// Heavy frame spike (30ms) -> DRS should scale down and VRS should go coarse (2x2)
	vrs.EvaluateFrame(30.0f);

	if (vrs.GetCurrentRenderScale() >= 1.0f) {
		std::fprintf(stderr, "FAIL: DRS failed to downscale on frame spike (Scale=%f)\n", vrs.GetCurrentRenderScale());
		std::exit(1);
	}

	if (vrs.GetCurrentShadingRate() != MetalShadingRate::Rate2x2) {
		std::fprintf(stderr, "FAIL: VRS failed to set Rate2x2 on frame spike\n");
		std::exit(1);
	}

	// Light frames (8ms) -> DRS should recover scale up and VRS should return to 1x1
	for (int i = 0; i < 20; ++i) {
		vrs.EvaluateFrame(8.0f);
	}

	if (vrs.GetCurrentRenderScale() < 0.99f) {
		std::fprintf(stderr, "FAIL: DRS failed to recover render scale to 1.0 (Scale=%f)\n", vrs.GetCurrentRenderScale());
		std::exit(1);
	}

	if (vrs.GetCurrentShadingRate() != MetalShadingRate::Rate1x1) {
		std::fprintf(stderr, "FAIL: VRS failed to recover Rate1x1\n");
		std::exit(1);
	}

	vrs.Shutdown();
	std::printf("  [ OK ] MetalVrs_DynamicAdjustment\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal VRS & DRS Pipeline Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalVrsLifecycle();
	TestDrsAndVrsDynamicAdjustment();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
