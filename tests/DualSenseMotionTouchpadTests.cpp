// DualSenseMotionTouchpadTests.cpp
//
// Unit & Integration Tests for DualSense Touchpad & 6-Axis Motion Subsystem.

#include "input/dualsenseMotionTouchpad.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Input;

static void TestTouchpadMultiTouch() {
	std::printf("[TEST] DualSense_MultiTouch\n");

	DualSenseMotionTouchpad pad;

	// Set finger 1 & finger 2
	pad.SetTouch(0, 500, 300, true, 1);
	pad.SetTouch(1, 1200, 800, true, 2);

	auto t1 = pad.GetTouch(0);
	auto t2 = pad.GetTouch(1);

	if (!t1.is_active || t1.x != 500 || t1.y != 300 || t1.touch_id != 1) {
		std::fprintf(stderr, "FAIL: Touch point 0 mismatch\n");
		std::exit(1);
	}

	if (!t2.is_active || t2.x != 1200 || t2.y != 800 || t2.touch_id != 2) {
		std::fprintf(stderr, "FAIL: Touch point 1 mismatch\n");
		std::exit(1);
	}

	// Test boundary clamping (2000 -> 1919)
	pad.SetTouch(0, 2500, 1500, true, 3);
	t1 = pad.GetTouch(0);
	if (t1.x != 1919 || t1.y != 1079) {
		std::fprintf(stderr, "FAIL: Touch boundary clamping failed (X=%u, Y=%u)\n", t1.x, t1.y);
		std::exit(1);
	}

	std::printf("  [ OK ] DualSense_MultiTouch\n");
}

static void TestMotionSensorData() {
	std::printf("[TEST] DualSense_MotionSensor\n");

	DualSenseMotionTouchpad pad;

	MotionSensorData motion{};
	motion.gyro_pitch_deg_s = 45.0f;
	motion.gyro_yaw_deg_s   = -90.0f;
	motion.gyro_roll_deg_s  = 15.0f;
	motion.accel_x_g        = 0.1f;
	motion.accel_y_g        = -0.2f;
	motion.accel_z_g        = 0.98f;

	pad.SetMotion(motion);

	auto readback = pad.GetMotion();
	if (readback.gyro_pitch_deg_s != 45.0f || readback.gyro_yaw_deg_s != -90.0f ||
	    readback.accel_z_g != 0.98f) {
		std::fprintf(stderr, "FAIL: Motion sensor readback data mismatch\n");
		std::exit(1);
	}

	const auto& stats = pad.GetStats();
	if (stats.total_motion_samples != 1) {
		std::fprintf(stderr, "FAIL: Motion sample stats mismatch: %llu\n", stats.total_motion_samples);
		std::exit(1);
	}

	std::printf("  [ OK ] DualSense_MotionSensor\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — DualSense Touchpad & Motion Sensor Test Suite\n");
	std::printf("================================================================================\n");

	TestTouchpadMultiTouch();
	TestMotionSensorData();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
