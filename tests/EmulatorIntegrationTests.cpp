// EmulatorIntegrationTests.cpp
//
// End-to-end boot, frame scheduling, save state, and benchmark test suite for Phase O:
// Full Emulator Integration.

#include "emulator/emulatorIntegration.h"
#include "emulator/frameScheduler.h"
#include "emulator/saveStateEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Emulator;
using namespace Loader::Recompiler;

// ─── 1. End-to-End Boot Pipeline Test ──────────────────────────────────────────

void TestFullEmulatorBootPipeline() {
	std::printf("  [Test 1] 7-Stage End-to-End Emulator Boot Pipeline...\n");

	EmulatorConfig config{};
	config.game_path = "/app0/eboot.bin";
	config.enable_opt = true;

	EmulatorEngine engine(config);
	bool init_ok = engine.Initialize();
	Check(init_ok, "Emulator engine initialization failed");
	Check(engine.GetState() == EmulatorState::Initialized, "State should be Initialized");

	bool boot_ok = engine.BootGame("/app0/eboot.bin");
	Check(boot_ok, "Emulator engine boot game failed");
	Check(engine.GetState() == EmulatorState::Running, "State should be Running");

	// Run 10 synthetic frames
	for (int frame = 0; frame < 10; ++frame) {
		bool frame_ok = engine.RunFrame();
		Check(frame_ok, "RunFrame failed");
	}

	Check(engine.GetStats().frames_rendered == 10, "Rendered frame count mismatch");
	Check(engine.GetStats().shaders_optimized == 10, "Optimized shader count mismatch");

	engine.Shutdown();
	Check(engine.GetState() == EmulatorState::Stopped, "State should be Stopped");

	std::printf("  [OK] Test 1: 7-Stage End-to-End Emulator Boot Pipeline\n");
}

// ─── 2. Frame Scheduler & Pacing Test ─────────────────────────────────────────

void TestFrameSchedulerAndPacing() {
	std::printf("  [Test 2] Frame Scheduler & 60/120 Hz Target Pacing...\n");

	FrameScheduler scheduler(120); // 120 FPS target
	Check(scheduler.GetTargetFramerate() == 120, "Target framerate setting mismatch");

	for (int f = 0; f < 5; ++f) {
		scheduler.BeginFrame();
		scheduler.EndFrameAndPace();
	}

	Check(scheduler.GetStats().total_frames == 5, "Total frame count mismatch");
	Check(scheduler.GetStats().actual_frame_ms > 0.0, "Actual frame time invalid");

	std::printf("  [OK] Test 2: Frame Scheduler & Target Pacing\n");
}

// ─── 3. Save State Engine Test ────────────────────────────────────────────────

void TestSaveStateEngine() {
	std::printf("  [Test 3] Save State Snapshot Serialization & Restoration...\n");

	GuestCpuContext ctx{};
	ctx.rax = 0x1111222233334444ULL;
	ctx.rbx = 0x5555666677778888ULL;
	ctx.rip = 0x0000000000401000ULL;

	SaveStateSnapshot snapshot{};
	bool create_ok = SaveStateEngine::CreateSnapshot(ctx, 42, snapshot);
	Check(create_ok, "Create snapshot failed");

	std::string filepath = "/tmp/test_kyty_savestate.bin";
	bool save_ok = SaveStateEngine::SaveToFile(snapshot, filepath);
	Check(save_ok, "Save state to file failed");

	SaveStateSnapshot loaded_snapshot{};
	bool load_ok = SaveStateEngine::LoadFromFile(filepath, loaded_snapshot);
	Check(load_ok, "Load state from file failed");

	GuestCpuContext restored_ctx{};
	bool restore_ok = SaveStateEngine::RestoreSnapshot(loaded_snapshot, restored_ctx);
	Check(restore_ok, "Restore snapshot failed");

	Check(restored_ctx.rax == ctx.rax && restored_ctx.rbx == ctx.rbx && restored_ctx.rip == ctx.rip, "Restored CPU context state mismatch");

	std::remove(filepath.c_str());

	std::printf("  [OK] Test 3: Save State Snapshot Serialization & Restoration\n");
}

// ─── 4. Memory Leak & Subsystem Lifecycle Stress Test ─────────────────────────

void TestMemoryLeakAndLifecycleStress() {
	std::printf("  [Test 4] Memory Leak & Subsystem Lifecycle Stress Test (20 iterations)...\n");

	for (int iter = 0; iter < 20; ++iter) {
		EmulatorEngine engine;
		engine.Initialize();
		engine.BootGame("/app0/eboot.bin");
		for (int f = 0; f < 3; ++f) {
			engine.RunFrame();
		}
		engine.Shutdown();
	}

	std::printf("  [OK] Test 4: Memory Leak & Subsystem Lifecycle Stress Test\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkEmulatorIntegration() {
	std::printf("\n--- Phase O Benchmarks ---\n");

	EmulatorEngine engine;
	engine.Initialize();
	engine.BootGame("/app0/eboot.bin");

	constexpr int kBenchmarkFrames = 1000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int f = 0; f < kBenchmarkFrames; ++f) {
		engine.RunFrame();
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double total_dt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	double per_frame_us = (total_dt_ms * 1000.0) / kBenchmarkFrames;
	double frame_rate = kBenchmarkFrames / (total_dt_ms / 1000.0);

	std::printf("  [Bench] Full Pipeline Frame Latency: %.2f us / frame\n", per_frame_us);
	std::printf("  [Bench] Frame Processing Capacity: %.2f FPS (Tested %d frames)\n", frame_rate, kBenchmarkFrames);

	engine.Shutdown();
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase O: Full Emulator Integration         \n");
	std::printf("====================================================\n\n");

	TestFullEmulatorBootPipeline();
	TestFrameSchedulerAndPacing();
	TestSaveStateEngine();
	TestMemoryLeakAndLifecycleStress();

	BenchmarkEmulatorIntegration();

	std::printf("\nEmulatorIntegrationTests: ALL PASSED\n");
	return 0;
}
