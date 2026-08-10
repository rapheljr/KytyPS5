// DualSenseHapticPhysicsTests.cpp
//
// Unit & Integration Tests for DualSense Adaptive Trigger Haptic Physics Engine.

#include "input/dualsenseHapticPhysics.h"

#include <cstdio>
#include <cstdlib>

using namespace Input;

static void TestTriggerRigidResistance() {
	std::printf("[TEST] DualSense_RigidResistance\n");

	DualSenseHapticPhysicsEngine engine;

	TriggerEffectConfig cfg{};
	cfg.mode = TriggerEffectMode::Rigid;
	cfg.start_position = 100;
	cfg.force = 200;

	engine.SetLeftTriggerEffect(cfg);

	// Below threshold -> 0 force
	if (engine.EvaluateLeftTriggerForce(50) != 0) {
		std::fprintf(stderr, "FAIL: Force non-zero below start position\n");
		std::exit(1);
	}

	// Above threshold -> 200 force
	if (engine.EvaluateLeftTriggerForce(150) != 200) {
		std::fprintf(stderr, "FAIL: Force mismatch above start position\n");
		std::exit(1);
	}

	std::printf("  [ OK ] DualSense_RigidResistance\n");
}

static void TestTriggerWeaponKickback() {
	std::printf("[TEST] DualSense_WeaponKickback\n");

	DualSenseHapticPhysicsEngine engine;

	TriggerEffectConfig cfg{};
	cfg.mode = TriggerEffectMode::WeaponKickback;
	cfg.start_position = 120;
	cfg.force = 240;

	engine.SetRightTriggerEffect(cfg);

	// Below threshold -> 0
	if (engine.EvaluateRightTriggerForce(80) != 0) {
		std::fprintf(stderr, "FAIL: Force non-zero before kickback point\n");
		std::exit(1);
	}

	// At wall -> peak resistance (240)
	if (engine.EvaluateRightTriggerForce(130) != 240) {
		std::fprintf(stderr, "FAIL: Peak wall force mismatch\n");
		std::exit(1);
	}

	// Past break point -> dropped resistance (60)
	if (engine.EvaluateRightTriggerForce(200) != 60) {
		std::fprintf(stderr, "FAIL: Post-break force mismatch: got %u, expected 60\n", engine.EvaluateRightTriggerForce(200));
		std::exit(1);
	}

	const auto& stats = engine.GetStats();
	if (stats.total_force_evaluations < 3) {
		std::fprintf(stderr, "FAIL: Stats evaluation count mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] DualSense_WeaponKickback\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — DualSense Adaptive Trigger Haptic Physics Test Suite\n");
	std::printf("================================================================================\n");

	TestTriggerRigidResistance();
	TestTriggerWeaponKickback();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
