// DualSenseHidReportTests.cpp
//
// Unit & Integration Tests for macOS IOKit DualSense HID Input & Output Report Engine.

#include "input/macOsHidBackend.h"

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

using namespace Libs::Input;

void TestDualSenseInputReportDecoding() {
	std::printf("[TEST] DualSense_InputReportDecoding\n");

	// Construct a synthetic 64-byte USB DualSense input report
	std::vector<uint8_t> report(64, 0);
	report[0] = 0x01; // Report ID

	// Sticks centered at 128 (0x80), but left stick tilted right (0xC0), right stick up (0x20)
	report[1] = 0xC0; // Left X
	report[2] = 0x80; // Left Y
	report[3] = 0x80; // Right X
	report[4] = 0x20; // Right Y

	// Triggers
	report[5] = 0xAA; // L2
	report[6] = 0xFF; // R2

	// Buttons
	// byte 8: DPadDown (4) | Cross (0x20) | Triangle (0x80) = 0xA4
	report[8] = 0xA4;
	// byte 9: L1 (0x01) | R1 (0x02) | Options (0x20) = 0x23
	report[9] = 0x23;
	// byte 10: PS (0x01) | Mute (0x04) = 0x05
	report[10] = 0x05;

	// IMU (gyro & accel)
	int16_t gyro_x = 1024; // 1.0 rad/s
	int16_t accel_z = 8192; // 1.0 g
	std::memcpy(report.data() + 16, &gyro_x, 2);
	std::memcpy(report.data() + 26, &accel_z, 2);

	DualSenseState state{};
	bool ok = MacOsHidBackend::DecodeDualSenseInputReport(report.data(), report.size(), state);
	Check(ok, "DecodeDualSenseInputReport failed");

	Check(state.connected, "Controller should be connected");
	Check(state.left_stick_x == 0xC0, "Left stick X mismatch");
	Check(state.right_stick_y == 0x20, "Right stick Y mismatch");
	Check(state.trigger_l2 == 0xAA, "Trigger L2 mismatch");
	Check(state.trigger_r2 == 0xFF, "Trigger R2 mismatch");

	uint32_t btn = static_cast<uint32_t>(state.buttons);
	Check(btn & static_cast<uint32_t>(DualSenseButtons::DPadDown), "Expected DPadDown");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::Cross), "Expected Cross");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::Triangle), "Expected Triangle");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::L1), "Expected L1");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::R1), "Expected R1");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::Options), "Expected Options");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::PS), "Expected PS");
	Check(btn & static_cast<uint32_t>(DualSenseButtons::Mute), "Expected Mute");

	Check(state.imu.gyro_x > 0.99f && state.imu.gyro_x < 1.01f, "Gyro X mismatch");
	Check(state.imu.accel_z > 0.99f && state.imu.accel_z < 1.01f, "Accel Z mismatch");

	std::printf("  [ OK ] DualSense_InputReportDecoding\n");
}

void TestDualSenseOutputReportEncoding() {
	std::printf("[TEST] DualSense_OutputReportEncoding\n");

	DualSenseState state{};
	state.haptics.small_motor_rumble = 0x40;
	state.haptics.large_motor_rumble = 0x80;

	state.lightbar.r = 0x12;
	state.lightbar.g = 0x34;
	state.lightbar.b = 0x56;

	state.trigger_l2_effect.mode           = AdaptiveTriggerMode::Feedback;
	state.trigger_l2_effect.start_position = 5;
	state.trigger_l2_effect.force_strength = 7;
	state.trigger_l2_effect.frequency      = 0;

	state.trigger_r2_effect.mode           = AdaptiveTriggerMode::Weapon;
	state.trigger_r2_effect.start_position = 2;
	state.trigger_r2_effect.force_strength = 8;
	state.trigger_r2_effect.frequency      = 10;

	uint8_t out_report[64] = {0};
	size_t report_len = MacOsHidBackend::EncodeDualSenseOutputReport(state, out_report, sizeof(out_report));
	Check(report_len >= 48, "EncodeDualSenseOutputReport produced invalid length");

	Check(out_report[0] == 0x02, "Report ID should be 0x02");
	Check(out_report[3] == 0x40, "Small motor rumble mismatch");
	Check(out_report[4] == 0x80, "Large motor rumble mismatch");

	Check(out_report[44] == 0x12, "Lightbar Red mismatch");
	Check(out_report[45] == 0x34, "Lightbar Green mismatch");
	Check(out_report[46] == 0x56, "Lightbar Blue mismatch");

	Check(out_report[11] == static_cast<uint8_t>(AdaptiveTriggerMode::Feedback), "L2 Trigger mode mismatch");
	Check(out_report[12] == 5, "L2 start pos mismatch");
	Check(out_report[13] == 7, "L2 force mismatch");

	Check(out_report[22] == static_cast<uint8_t>(AdaptiveTriggerMode::Weapon), "R2 Trigger mode mismatch");
	Check(out_report[23] == 2, "R2 start pos mismatch");
	Check(out_report[24] == 8, "R2 force mismatch");
	Check(out_report[25] == 10, "R2 freq mismatch");

	std::printf("  [ OK ] DualSense_OutputReportEncoding\n");
}

void TestMacOsHidBackendLifecycle() {
	std::printf("[TEST] MacOsHidBackend_Lifecycle\n");

	MacOsHidBackend backend;
	bool init_ok = backend.Initialize();
	Check(init_ok, "MacOsHidBackend Initialize failed");

	DualSenseState state{};
	bool poll_ok = backend.PollInput(0, state);
	Check(poll_ok, "PollInput failed");

	bool send_ok = backend.SendOutputReport(0, state);
	Check(send_ok, "SendOutputReport failed");

	backend.Shutdown();

	std::printf("  [ OK ] MacOsHidBackend_Lifecycle\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — macOS IOKit DualSense HID Haptics & Input Test Suite\n");
	std::printf("================================================================================\n");

	TestDualSenseInputReportDecoding();
	TestDualSenseOutputReportEncoding();
	TestMacOsHidBackendLifecycle();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
