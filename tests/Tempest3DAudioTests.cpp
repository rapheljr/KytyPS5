// Tempest3DAudioTests.cpp
//
// Unit & Integration Tests for Tempest 3D Audio HRTF Spatial DSP Engine.

#include "audio/tempest3DAudioEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Audio;

static void TestTempest3DAudioLifecycle() {
	std::printf("[TEST] Tempest3DAudio_Lifecycle\n");

	Tempest3DAudioEngine engine;
	if (!engine.Initialize(48000, 512)) {
		std::fprintf(stderr, "FAIL: Tempest3DAudioEngine initialization failed\n");
		std::exit(1);
	}

	if (!engine.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	engine.Shutdown();
	if (engine.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] Tempest3DAudio_Lifecycle\n");
}

static void TestTempest3DBinauralPanning() {
	std::printf("[TEST] Tempest3DAudio_BinauralPanning\n");

	Tempest3DAudioEngine engine;
	engine.Initialize(48000, 512);

	// Create hard-left emitter (x = -5.0f, y = 0, z = 0)
	SpatialPosition3D left_pos{-5.0f, 0.0f, 0.0f};
	uint32_t left_emitter = engine.CreateEmitter(left_pos, 1.0f);

	std::vector<float> mono(512, 1.0f);
	std::vector<float> left_out(512, 0.0f);
	std::vector<float> right_out(512, 0.0f);

	engine.ProcessBinauralHrtf(left_emitter, mono.data(), left_out.data(), right_out.data(), 512);

	// Hard left emitter must result in Left channel being significantly louder than Right channel
	if (left_out[0] <= right_out[0]) {
		std::fprintf(stderr, "FAIL: Left ear volume (%f) was not greater than Right ear (%f) for left source\n",
		             left_out[0], right_out[0]);
		std::exit(1);
	}

	// Move emitter to hard-right (x = +5.0f)
	SpatialPosition3D right_pos{5.0f, 0.0f, 0.0f};
	engine.SetEmitterPosition(left_emitter, right_pos);
	engine.ProcessBinauralHrtf(left_emitter, mono.data(), left_out.data(), right_out.data(), 512);

	if (right_out[0] <= left_out[0]) {
		std::fprintf(stderr, "FAIL: Right ear volume (%f) was not greater than Left ear (%f) for right source\n",
		             right_out[0], left_out[0]);
		std::exit(1);
	}

	const auto& stats = engine.GetStats();
	if (stats.total_emitters_rendered != 2 || stats.total_samples_convolved != 1024) {
		std::fprintf(stderr, "FAIL: Stats mismatch (Emitters=%u, Samples=%llu)\n",
		             stats.total_emitters_rendered, stats.total_samples_convolved);
		std::exit(1);
	}

	engine.Shutdown();
	std::printf("  [ OK ] Tempest3DAudio_BinauralPanning\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Tempest 3D Audio HRTF DSP Engine Test Suite\n");
	std::printf("================================================================================\n");

	TestTempest3DAudioLifecycle();
	TestTempest3DBinauralPanning();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
