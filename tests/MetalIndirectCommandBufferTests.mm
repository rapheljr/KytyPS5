// MetalIndirectCommandBufferTests.mm
//
// Unit tests for Metal Indirect Command Buffer (ICB) & Multi-Draw Indirect.

#include "graphics/host_gpu/renderer/backend/metalIndirectCommandBuffer.h"
#include <cstdio>
#include <cstdlib>

#define ASSERT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			::printf("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
			::exit(1); \
		} \
	} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static void Test_MetalIcb_Lifecycle() {
	::printf("[TEST] MetalIcb_Lifecycle\n");
	HostGpu::Backend::MetalIndirectCommandBuffer icb;
	ASSERT_FALSE(icb.IsInitialized());

	HostGpu::Backend::MetalIcbConfig config;
	config.max_command_count = 512;
	config.inherit_buffers = true;
	config.inherit_pipeline_state = true;

	ASSERT_TRUE(icb.Initialize(config));
	ASSERT_TRUE(icb.IsInitialized());
	ASSERT_EQ(icb.GetMaxCommandCount(), 512U);
	ASSERT_EQ(icb.GetRecordedCommandCount(), 0U);

	icb.Shutdown();
	ASSERT_FALSE(icb.IsInitialized());
	::printf("  [ OK ] MetalIcb_Lifecycle\n");
}

static void Test_MetalIcb_CommandRecordingAndExecution() {
	::printf("[TEST] MetalIcb_CommandRecordingAndExecution\n");
	HostGpu::Backend::MetalIndirectCommandBuffer icb;

	HostGpu::Backend::MetalIcbConfig config;
	config.max_command_count = 128;
	ASSERT_TRUE(icb.Initialize(config));

	// Record DrawIndexed command
	bool ok1 = icb.SetDrawIndexed(0, 36, 1, 0, 0, 0);
	ASSERT_TRUE(ok1);

	// Record Draw non-indexed command
	bool ok2 = icb.SetDraw(1, 100, 2, 0, 0);
	ASSERT_TRUE(ok2);

	// Out of bounds index test
	bool ok_oob = icb.SetDraw(200, 10, 1, 0, 0);
	ASSERT_FALSE(ok_oob);

	ASSERT_EQ(icb.GetRecordedCommandCount(), 2U);

	// Test execute mock
	int dummy_encoder = 1;
	ASSERT_TRUE(icb.Execute(&dummy_encoder));

	// Reset commands
	icb.Reset();
	ASSERT_EQ(icb.GetRecordedCommandCount(), 0U);

	::printf("  [ OK ] MetalIcb_CommandRecordingAndExecution\n");
}

int main() {
	::printf("================================================================================\n");
	::printf("  KytyPS5 — Metal Indirect Command Buffer Test Suite\n");
	::printf("================================================================================\n");

	Test_MetalIcb_Lifecycle();
	Test_MetalIcb_CommandRecordingAndExecution();

	::printf("================================================================================\n");
	::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	::printf("================================================================================\n");
	return 0;
}
