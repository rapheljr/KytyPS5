// JitToMetalEndToEndTests.mm
//
// Native Apple Silicon End-to-End JIT CPU to Metal GPU Verification Test Suite.
// Verifies:
//   1. Guest x86 JIT compilation & execution producing GPU draw arguments.
//   2. PM4 packet stream decoding & translation to Metal render command encoders.
//   3. Command buffer submission to MTLCommandQueue with GPU execution time measurement.
//   4. Multi-frame presentation loop with MetalFrameSync triple-buffering.

#include "emulator/emulatorIntegration.h"
#include "graphics/guest_gpu/command_processor/pm4Translator.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <chrono>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Graphics;
using namespace Libs::Graphics::Pm4;
using namespace Loader::Recompiler;
using namespace Emulator;

void TestJitToMetalDrawExecution() {
	std::printf("[TEST] JitToMetal_DrawExecution\n");

	// 1. Initialize Metal Backend
	MetalGraphicBackend backend;
	bool ok = backend.Initialize();
	Check(ok, "MetalGraphicBackend initialization failed");
	Check(backend.IsSupported(), "Metal not supported on host device");

	// 2. Guest JIT execution: compute vertex parameters in guest registers
	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x500000;

	// Machine code:
	//   mov eax, 12        ; 12 triangles (36 vertices)
	//   mov ebx, 3         ; 3 vertices per triangle
	//   imul eax, ebx      ; eax = 36 (vertex count)
	//   ret
	const uint8_t code[] = {
		0xB8, 0x0C, 0x00, 0x00, 0x00,
		0xBB, 0x03, 0x00, 0x00, 0x00,
		0x0F, 0xAF, 0xC3,
		0xC3
	};

	bool jit_ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(jit_ok, "JIT block execution failed");
	Check(ctx.rax == 36, "JIT vertex count computation failed (expected 36)");

	std::printf("  Guest JIT Computed Vertex Count = %llu\n", (unsigned long long)ctx.rax);

	// 3. PM4 Command Stream Generation & Translation
	Pm4Translator translator(&backend);

	Pm4CommandList cmd_list;

	// Clear render target to cornflower blue
	CmdClearRenderTarget clear_cmd{};
	clear_cmd.color[0] = 0.392f;
	clear_cmd.color[1] = 0.584f;
	clear_cmd.color[2] = 0.929f;
	clear_cmd.color[3] = 1.0f;
	clear_cmd.clear_color = true;
	cmd_list.AddCommand(clear_cmd);

	// Draw 36 vertices (cube)
	CmdDrawNonIndexed draw_cmd{};
	draw_cmd.first_vertex   = 0;
	draw_cmd.vertex_count   = static_cast<uint32_t>(ctx.rax);
	draw_cmd.instance_count = 1;
	cmd_list.AddCommand(draw_cmd);

	// Translate PM4 commands to Metal
	bool trans_ok = translator.TranslateAndExecute(cmd_list);
	Check(trans_ok, "PM4 to Metal translation failed");

	auto* cb = backend.GetActiveCommandBuffer();
	Check(cb != nullptr, "No active Metal command buffer created");
	Check(cb->IsValid(), "Active command buffer is invalid");
	Check(cb->GetState() == MetalCommandBufferState::Recording, "Command buffer not in Recording state");

	// 4. Commit and wait for GPU completion
	backend.EndRenderPass();
	bool submit_ok = backend.SubmitCurrentCommandBuffer();
	Check(submit_ok, "SubmitCurrentCommandBuffer failed");

	cb->WaitUntilCompleted();
	Check(cb->GetState() == MetalCommandBufferState::Completed, "Command buffer not in Completed state");

	uint64_t gpu_ns = cb->GetGpuExecutionTimeNs();
	std::printf("  Metal GPU Execution Time = %llu ns (%.3f ms)\n",
	            (unsigned long long)gpu_ns, static_cast<double>(gpu_ns) / 1000000.0);

	const auto& stats = translator.GetStats();
	Check(stats.clear_commands == 1, "Expected 1 clear command");
	Check(stats.draw_commands == 1, "Expected 1 draw command");

	std::printf("  [ OK ] JitToMetal_DrawExecution\n");
}

void TestJitToMetalMultiFramePresentation() {
	std::printf("[TEST] JitToMetal_MultiFramePresentation (10 Frames)\n");

	EmulatorConfig config;
	config.target_framerate = 60;
	config.enable_opt       = true;

	EmulatorEngine engine(config);
	bool init_ok = engine.Initialize();
	Check(init_ok, "EmulatorEngine initialization failed");

	bool boot_ok = engine.BootGame("/app0/eboot.bin");
	Check(boot_ok, "EmulatorEngine BootGame failed");
	Check(engine.GetState() == EmulatorState::Running, "Engine not in Running state");

	// Run 10 frame iterations through JIT dispatch + Metal presentation
	for (int frame = 0; frame < 10; ++frame) {
		bool frame_ok = engine.RunFrame();
		Check(frame_ok, "RunFrame failed");
	}

	const auto& stats = engine.GetStats();
	std::printf("  Frames Rendered      = %llu\n", (unsigned long long)stats.frames_rendered);
	std::printf("  PM4 Packets Decoded  = %llu\n", (unsigned long long)stats.pm4_packets_decoded);
	std::printf("  Shaders Optimized    = %llu\n", (unsigned long long)stats.shaders_optimized);
	std::printf("  Avg Frame Time (ms)  = %.3f ms\n", stats.avg_frame_time_ms);

	Check(stats.frames_rendered == 10, "Expected 10 frames rendered");
	Check(stats.pm4_packets_decoded >= 10, "Expected >= 10 PM4 packets decoded");

	engine.Shutdown();
	Check(engine.GetState() == EmulatorState::Stopped, "Engine not Stopped after shutdown");

	std::printf("  [ OK ] JitToMetal_MultiFramePresentation\n");
}

void TestJitToMetalIndexedDrawAndSync() {
	std::printf("[TEST] JitToMetal_IndexedDrawAndSync\n");

	MetalGraphicBackend backend;
	bool ok = backend.Initialize();
	Check(ok, "MetalGraphicBackend init failed");

	Pm4Translator translator(&backend);

	Pm4CommandList cmd_list;

	// Surface sync barrier
	CmdSurfaceSync sync_cmd{};
	sync_cmd.cp_coher_cntl = 0x1;
	cmd_list.AddCommand(sync_cmd);

	// Indexed draw
	CmdDrawIndexed draw_cmd{};
	draw_cmd.index_count    = 36;
	draw_cmd.index_gpu_addr = 0x10000000;
	draw_cmd.first_index    = 0;
	draw_cmd.instance_count = 1;
	cmd_list.AddCommand(draw_cmd);

	bool trans_ok = translator.TranslateAndExecute(cmd_list);
	Check(trans_ok, "TranslateAndExecute failed for indexed draw & sync");

	backend.PresentFrame(0);

	const auto& stats = translator.GetStats();
	Check(stats.barrier_commands == 1, "Expected 1 barrier command");
	Check(stats.draw_commands == 1, "Expected 1 draw command");

	std::printf("  [ OK ] JitToMetal_IndexedDrawAndSync\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::setbuf(stderr, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — End-to-End JIT CPU to Metal GPU Verification Test Suite\n");
	std::printf("================================================================================\n");

	TestJitToMetalDrawExecution();
	TestJitToMetalMultiFramePresentation();
	TestJitToMetalIndexedDrawAndSync();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	std::fflush(stdout);
	return 0;
}
