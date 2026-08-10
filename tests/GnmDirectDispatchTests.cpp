// GnmDirectDispatchTests.cpp
//
// Unit & Integration Tests for GNM / AGC Graphics Driver Direct Command Buffer Dispatch.
// Verifies:
//   1. Multi-DCB command stream submission (GraphicsDriverSubmitMultiDcbs).
//   2. PM4 packet unpacking (Draw, SetState, SurfaceSync).
//   3. VideoOutSubmitFlip integration with display buffer presentation.

#include "graphics/guest_gpu/command_processor/pm4CommandList.h"
#include "graphics/guest_gpu/command_processor/pm4Translator.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "kernel/openOrbisSubsystems.h"
#include "libs/agc.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Graphics;
using namespace Libs::Graphics::Pm4;
using namespace Libs::Graphics::Gen5Driver;

void TestGnmMultiDcbSubmission() {
	std::printf("[TEST] Gnm_MultiDcbSubmission\n");

	// Create 2 mock DCBs containing valid PM4 type-3 packets
	std::vector<uint32_t> dcb0 = {
		KYTY_PM4(4, IT_NOP, R_ZERO), 0x11223344, 0x55667788, 0x99AABBCC,
		KYTY_PM4(3, IT_NOP, R_ZERO), 0xDEADBEEF, 0xFEEDFACE
	};

	// DCB 1: NOP packet
	std::vector<uint32_t> dcb1 = {
		KYTY_PM4(3, IT_NOP, R_ZERO), 0xCAFEBABE, 0x00000001
	};

	uint32_t* dcb_addrs[2] = { dcb0.data(), dcb1.data() };
	uint32_t  dcb_sizes[2] = { static_cast<uint32_t>(dcb0.size()), static_cast<uint32_t>(dcb1.size()) };

	int res = GraphicsDriverSubmitMultiDcbs(dcb_addrs, dcb_sizes, 2);
	Check(res == 0, "GraphicsDriverSubmitMultiDcbs failed for 2 valid DCBs");

	// Test zero count
	res = GraphicsDriverSubmitMultiDcbs(dcb_addrs, dcb_sizes, 0);
	Check(res == 0, "GraphicsDriverSubmitMultiDcbs failed for 0 count");

	// Test null pointers
	res = GraphicsDriverSubmitMultiDcbs(nullptr, dcb_sizes, 2);
	Check(res != 0, "Expected error on null dcb_gpu_addrs");

	std::printf("  [ OK ] Gnm_MultiDcbSubmission\n");
}

void TestGnmPm4CommandListTranslation() {
	std::printf("[TEST] Gnm_Pm4CommandListTranslation\n");

	MetalGraphicBackend backend;
	bool ok = backend.Initialize();
	Check(ok, "Metal backend initialization failed");

	Pm4Translator translator(&backend);
	Pm4CommandList list;

	// Add clear command
	CmdClearRenderTarget clear{};
	clear.color[0] = 0.1f; clear.color[1] = 0.2f; clear.color[2] = 0.3f; clear.color[3] = 1.0f;
	clear.clear_color = true;
	list.AddCommand(clear);

	// Add draw non-indexed
	CmdDrawNonIndexed draw{};
	draw.vertex_count = 144;
	draw.instance_count = 2;
	draw.first_vertex = 0;
	list.AddCommand(draw);

	// Add pipeline barrier
	CmdPipelineBarrier barrier{};
	barrier.flush_cb = true;
	barrier.inv_l2   = true;
	list.AddCommand(barrier);

	bool trans_ok = translator.TranslateAndExecute(list);
	Check(trans_ok, "PM4 translation failed");

	const auto& stats = translator.GetStats();
	Check(stats.clear_commands == 1, "Expected 1 clear command");
	Check(stats.draw_commands == 1, "Expected 1 draw command");
	Check(stats.barrier_commands == 1, "Expected 1 barrier command");

	backend.PresentFrame(0);

	std::printf("  [ OK ] Gnm_Pm4CommandListTranslation\n");
}

void TestSubsystemHubGnmStubs() {
	std::printf("[TEST] SubsystemHub_GnmStubs\n");

	Loader::Recompiler::JitTelemetryCollector telemetry;
	Kernel::OpenOrbisSubsystemHub hub(telemetry);
	hub.RegisterAll();

	// Test sceGnmSubmitCommandBuffers stub via Dispatch
	std::vector<uint32_t> dcb = { KYTY_PM4(2, IT_NOP, R_ZERO) };
	uint32_t* addrs[1] = { dcb.data() };
	uint32_t  sizes[1] = { static_cast<uint32_t>(dcb.size()) };

	Kernel::SubsystemCallCtx ctx{};
	ctx.arg0 = 1;
	ctx.arg1 = reinterpret_cast<uint64_t>(addrs);
	ctx.arg2 = reinterpret_cast<uint64_t>(sizes);

	int64_t ret = hub.Dispatch("sceGnmSubmitCommandBuffers", ctx);
	Check(ret == Kernel::SCE_OK, "Dispatch sceGnmSubmitCommandBuffers returned non-zero");

	// Test sceVideoOutSubmitFlip stub via Dispatch
	ctx.arg0 = 0; // video handle
	ctx.arg1 = 1; // frame buffer index
	ret = hub.Dispatch("sceVideoOutSubmitFlip", ctx);
	Check(ret == Kernel::SCE_OK, "Dispatch sceVideoOutSubmitFlip returned non-zero");

	std::printf("  [ OK ] SubsystemHub_GnmStubs\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — GNM / AGC Graphics Driver Direct Dispatch Test Suite\n");
	std::printf("================================================================================\n");

	TestGnmMultiDcbSubmission();
	TestGnmPm4CommandListTranslation();
	TestSubsystemHubGnmStubs();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
