// MetalFxPipelineTests.mm
//
// Unit tests for MetalFX Super Resolution Spatial & Temporal Pipeline.

#include "graphics/host_gpu/renderer/backend/metalFxPipeline.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>

#define ASSERT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			::printf("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
			::exit(1); \
		} \
	} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_FLOAT_EQ(a, b) ASSERT_TRUE(std::fabs((a) - (b)) < 0.001f)

static void Test_MetalFx_Lifecycle() {
	::printf("[TEST] MetalFx_Lifecycle\n");
	HostGpu::Backend::MetalFxPipeline pipeline;
	ASSERT_FALSE(pipeline.IsInitialized());

	HostGpu::Backend::MetalFxConfig config;
	config.mode = HostGpu::Backend::MetalFxUpscalingMode::Spatial;
	config.preset = HostGpu::Backend::MetalFxQualityPreset::Quality;
	config.input_width = 1920;
	config.input_height = 1080;
	config.output_width = 3840;
	config.output_height = 2160;

	bool ok = pipeline.Initialize(config);
	ASSERT_TRUE(ok);
	ASSERT_TRUE(pipeline.IsInitialized());
	ASSERT_FLOAT_EQ(pipeline.GetScalingFactor(), 2.0f);
	ASSERT_EQ(pipeline.GetProcessedFrameCount(), 0ULL);

	pipeline.Shutdown();
	ASSERT_FALSE(pipeline.IsInitialized());
	::printf("  [ OK ] MetalFx_Lifecycle\n");
}

static void Test_MetalFx_ScalingAndEncoding() {
	::printf("[TEST] MetalFx_ScalingAndEncoding\n");
	HostGpu::Backend::MetalFxPipeline pipeline;

	HostGpu::Backend::MetalFxConfig config;
	config.mode = HostGpu::Backend::MetalFxUpscalingMode::Temporal;
	config.preset = HostGpu::Backend::MetalFxQualityPreset::Balanced;
	config.input_width = 2560;
	config.input_height = 1440;
	config.output_width = 3840;
	config.output_height = 2160;
	config.jitter_offset_x = 0.25f;
	config.jitter_offset_y = -0.25f;

	ASSERT_TRUE(pipeline.Initialize(config));
	ASSERT_FLOAT_EQ(pipeline.GetScalingFactor(), 1.5f);

	// Test encode simulated null/mock safety checks
	ASSERT_FALSE(pipeline.EncodeSpatialUpscale(nullptr, nullptr, nullptr));
	ASSERT_FALSE(pipeline.EncodeTemporalUpscale(nullptr, nullptr, nullptr, nullptr, nullptr));

	// Test encode with dummy valid pointer addresses
	int dummy_cmd = 1;
	int dummy_color = 2;
	int dummy_depth = 3;
	int dummy_motion = 4;
	int dummy_out = 5;

	bool encoded = pipeline.EncodeTemporalUpscale(&dummy_cmd, &dummy_color, &dummy_depth, &dummy_motion, &dummy_out);
	ASSERT_TRUE(encoded);
	ASSERT_EQ(pipeline.GetProcessedFrameCount(), 1ULL);

	::printf("  [ OK ] MetalFx_ScalingAndEncoding\n");
}

int main() {
	::printf("================================================================================\n");
	::printf("  KytyPS5 — MetalFX Super Resolution Pipeline Test Suite\n");
	::printf("================================================================================\n");

	Test_MetalFx_Lifecycle();
	Test_MetalFx_ScalingAndEncoding();

	::printf("================================================================================\n");
	::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	::printf("================================================================================\n");
	return 0;
}
