// MetalHdrPipelineTests.mm
//
// Unit & Integration Tests for Metal HDR Tone-Mapping and Display Color Pipeline.

#include "graphics/host_gpu/renderer/backend/metalHdrPipeline.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Graphics::HostGpu;

static void TestMetalHdrLifecycle() {
	std::printf("[TEST] MetalHdr_Lifecycle\n");

	MetalHdrPipeline pipeline;
	if (!pipeline.Initialize()) {
		std::fprintf(stderr, "FAIL: MetalHdrPipeline initialization failed\n");
		std::exit(1);
	}

	if (!pipeline.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	pipeline.Shutdown();
	if (pipeline.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MetalHdr_Lifecycle\n");
}

static void TestAcesFilmicToneMapping() {
	std::printf("[TEST] MetalHdr_AcesFilmicToneMapping\n");

	MetalHdrPipeline pipeline;
	HdrDisplayConfig cfg{};
	cfg.tone_mapper = ToneMappingOperator::ACESFilmic;
	cfg.exposure = 1.0f;
	pipeline.Initialize(cfg);

	// Test high dynamic range pixel [10.0, 5.0, 2.0, 1.0] -> should be tone mapped to (0.0, 1.0]
	std::vector<float> in_pixels = {10.0f, 5.0f, 2.0f, 1.0f};
	std::vector<float> out_pixels(4, 0.0f);

	pipeline.ProcessFrame(in_pixels.data(), out_pixels.data(), 1);

	if (out_pixels[0] <= 0.0f || out_pixels[0] > 1.0f ||
	    out_pixels[1] <= 0.0f || out_pixels[1] > 1.0f ||
	    out_pixels[2] <= 0.0f || out_pixels[2] > 1.0f) {
		std::fprintf(stderr, "FAIL: Tone-mapped output out of [0, 1] range: R=%f, G=%f, B=%f\n",
		             out_pixels[0], out_pixels[1], out_pixels[2]);
		std::exit(1);
	}

	// Preserved alpha
	if (out_pixels[3] != 1.0f) {
		std::fprintf(stderr, "FAIL: Alpha channel modified: %f\n", out_pixels[3]);
		std::exit(1);
	}

	const auto& stats = pipeline.GetStats();
	if (stats.total_frames_processed != 1) {
		std::fprintf(stderr, "FAIL: Total frames processed mismatch: %llu\n", stats.total_frames_processed);
		std::exit(1);
	}

	pipeline.Shutdown();
	std::printf("  [ OK ] MetalHdr_AcesFilmicToneMapping\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal HDR Tone-Mapping & Color Pipeline Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalHdrLifecycle();
	TestAcesFilmicToneMapping();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
