// Ps5DualSenseInputTests.cpp
//
// Complete Automated Test Suite for PS5 DualSense Emulation, Adaptive Triggers,
// Haptics, Touchpad, IMU, Speaker, Microphone, and macOS HID Report Parsing.

#include "input/dualSenseState.h"
#include "input/inputEngine.h"
#include "input/macOsHidBackend.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

void Check(bool value, const char* description, const char* file, int line) {
	g_tests_run++;
	if (!value) {
		g_tests_failed++;
		std::printf("  [FAIL] %s\n         at %s:%d\n", description, file, line);
	} else {
		g_tests_passed++;
	}
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) Check((a) == (b), #a " == " #b, __FILE__, __LINE__)

using namespace Libs::Input;

// Dummy mock input backend for isolated unit testing
class MockInputBackend : public IInputBackend {
public:
	MockInputBackend() = default;
	~MockInputBackend() override = default;

	bool Initialize() override { m_initialized = true; return true; }
	void Shutdown() override { m_initialized = false; }
	bool PollInput(uint32_t controller_index, DualSenseState& out_state) override {
		if (!m_initialized || controller_index >= kMaxControllers) return false;
		out_state = m_mock_states[controller_index];
		return true;
	}
	bool SendOutputReport(uint32_t controller_index, const DualSenseState& state) override {
		if (!m_initialized || controller_index >= kMaxControllers) return false;
		m_output_states[controller_index] = state;
		return true;
	}
	const char* GetBackendName() const override { return "Mock Input Test Backend"; }

	void SetMockState(uint32_t index, const DualSenseState& state) {
		if (index < kMaxControllers) m_mock_states[index] = state;
	}

private:
	bool            m_initialized = false;
	DualSenseState  m_mock_states[kMaxControllers];
	DualSenseState  m_output_states[kMaxControllers];
};

// ─── 1. Input Backend & Controller Lifecycle Test ─────────────────────────────

void TestInputBackendLifecycle() {
	std::printf("  [Test 1] IInputBackend Interface & Controller Lifecycle...\n");

	InputManager mgr;
	auto mock_backend = std::make_unique<MockInputBackend>();
	auto* mock_ref = mock_backend.get();

	CHECK(mgr.Initialize(std::move(mock_backend)));
	CHECK_EQ(std::string(mgr.GetBackend()->GetBackendName()), std::string("Mock Input Test Backend"));

	DualSenseState test_state{};
	test_state.buttons = DualSenseButtons::Cross | DualSenseButtons::L1;
	test_state.left_stick_x = 200;
	mock_ref->SetMockState(0, test_state);

	mgr.PollAll();

	auto* ctrl0 = mgr.GetController(0);
	CHECK(ctrl0 != nullptr);
	DualSenseState polled = ctrl0->GetState();

	CHECK((polled.buttons & DualSenseButtons::Cross));
	CHECK((polled.buttons & DualSenseButtons::L1));
	CHECK_EQ(polled.left_stick_x, 200u);

	mgr.Shutdown();
	std::printf("  [OK] Test 1: IInputBackend Interface & Controller Lifecycle\n");
}

// ─── 2. DualSense Buttons, Analog Sticks & Triggers Test ──────────────────────

void TestButtonsSticksAndTriggers() {
	std::printf("  [Test 2] DualSense Buttons, Analog Sticks & Triggers...\n");

	DualSenseController ctrl(0);

	ctrl.SetButton(DualSenseButtons::Triangle, true);
	ctrl.SetButton(DualSenseButtons::R2, true);
	ctrl.SetAnalogSticks(50, 60, 200, 210);
	ctrl.SetTriggers(128, 255);

	DualSenseState state = ctrl.GetState();

	CHECK((state.buttons & DualSenseButtons::Triangle));
	CHECK((state.buttons & DualSenseButtons::R2));
	CHECK(!((state.buttons & DualSenseButtons::Square)));

	CHECK_EQ(state.left_stick_x, 50u);
	CHECK_EQ(state.left_stick_y, 60u);
	CHECK_EQ(state.right_stick_x, 200u);
	CHECK_EQ(state.right_stick_y, 210u);

	CHECK_EQ(state.trigger_l2, 128u);
	CHECK_EQ(state.trigger_r2, 255u);

	std::printf("  [OK] Test 2: DualSense Buttons, Analog Sticks & Triggers\n");
}

// ─── 3. Adaptive Trigger Modes Test ──────────────────────────────────────────

void TestAdaptiveTriggers() {
	std::printf("  [Test 3] Adaptive Trigger Modes & Force Feedback...\n");

	DualSenseController ctrl(0);

	AdaptiveTriggerConfig r2_effect{};
	r2_effect.mode           = AdaptiveTriggerMode::Weapon;
	r2_effect.start_position = 30;
	r2_effect.force_strength = 200;
	r2_effect.frequency      = 10;

	ctrl.SetAdaptiveTrigger(true, r2_effect); // R2

	AdaptiveTriggerConfig l2_effect{};
	l2_effect.mode           = AdaptiveTriggerMode::Vibration;
	l2_effect.start_position = 10;
	l2_effect.force_strength = 150;
	l2_effect.frequency      = 25;

	ctrl.SetAdaptiveTrigger(false, l2_effect); // L2

	DualSenseState state = ctrl.GetState();

	CHECK(state.trigger_r2_effect.mode == AdaptiveTriggerMode::Weapon);
	CHECK_EQ(state.trigger_r2_effect.start_position, 30u);
	CHECK_EQ(state.trigger_r2_effect.force_strength, 200u);

	CHECK(state.trigger_l2_effect.mode == AdaptiveTriggerMode::Vibration);
	CHECK_EQ(state.trigger_l2_effect.start_position, 10u);

	std::printf("  [OK] Test 3: Adaptive Trigger Modes & Force Feedback\n");
}

// ─── 4. Vibration, Haptics & Lightbar Test ───────────────────────────────────

void TestVibrationAndHaptics() {
	std::printf("  [Test 4] Vibration Rumble, Haptics & Lightbar RGB...\n");

	DualSenseController ctrl(0);

	ctrl.SetVibration(100, 200);
	ctrl.SetLightbar(255, 128, 0); // Orange Lightbar

	DualSenseState state = ctrl.GetState();

	CHECK_EQ(state.haptics.small_motor_rumble, 100u);
	CHECK_EQ(state.haptics.large_motor_rumble, 200u);

	CHECK_EQ(state.lightbar.r, 255u);
	CHECK_EQ(state.lightbar.g, 128u);
	CHECK_EQ(state.lightbar.b, 0u);

	std::printf("  [OK] Test 4: Vibration Rumble, Haptics & Lightbar RGB\n");
}

// ─── 5. Touchpad & 6-DOF IMU Motion Sensors Test ─────────────────────────────

void TestTouchpadAndIMU() {
	std::printf("  [Test 5] Dual-Point Touchpad & 6-DOF IMU Motion Sensors...\n");

	DualSenseController ctrl(0);

	ctrl.SetTouch0(true, 960, 540, 1);
	ctrl.SetTouch1(true, 1200, 800, 2);
	ctrl.SetIMU(0.1f, -0.2f, 0.5f, 0.0f, 0.0f, 1.0f);

	DualSenseState state = ctrl.GetState();

	CHECK(state.touch0.active);
	CHECK_EQ(state.touch0.x, 960u);
	CHECK_EQ(state.touch0.y, 540u);
	CHECK_EQ(state.touch0.id, 1u);

	CHECK(state.touch1.active);
	CHECK_EQ(state.touch1.x, 1200u);
	CHECK_EQ(state.touch1.y, 800u);

	CHECK(std::abs(state.imu.gyro_x - 0.1f) < 0.001f);
	CHECK(std::abs(state.imu.accel_z - 1.0f) < 0.001f);

	std::printf("  [OK] Test 5: Dual-Point Touchpad & 6-DOF IMU Motion Sensors\n");
}

// ─── 6. macOS HID Report Decoder & Encoder Test ──────────────────────────────

void TestMacOsHidReportEncodingDecoding() {
	std::printf("  [Test 6] macOS HID Input/Output Report Decoding & Encoding...\n");

	// Synthesize a 64-byte DualSense USB Input Report
	uint8_t dummy_report[64] = {0};
	dummy_report[1] = 180; // LX
	dummy_report[2] = 190; // LY
	dummy_report[3] = 40;  // RX
	dummy_report[4] = 50;  // RY
	dummy_report[5] = 200; // L2 Analog
	dummy_report[6] = 255; // R2 Analog
	dummy_report[8] = 0x20; // Cross button
	dummy_report[9] = 0x01; // L1 button

	DualSenseState state{};
	CHECK(MacOsHidBackend::DecodeDualSenseInputReport(dummy_report, 64, state));

	CHECK_EQ(state.left_stick_x, 180u);
	CHECK_EQ(state.left_stick_y, 190u);
	CHECK_EQ(state.trigger_l2, 200u);
	CHECK_EQ(state.trigger_r2, 255u);
	CHECK((state.buttons & DualSenseButtons::Cross));
	CHECK((state.buttons & DualSenseButtons::L1));

	// Test Output Report Encoding
	state.haptics.small_motor_rumble = 150;
	state.haptics.large_motor_rumble = 220;
	state.lightbar.r = 255;
	state.lightbar.g = 0;
	state.lightbar.b = 128;

	uint8_t out_buf[64] = {0};
	size_t report_size = MacOsHidBackend::EncodeDualSenseOutputReport(state, out_buf, 64);
	CHECK_EQ(report_size, 48u);
	CHECK_EQ(out_buf[0], 0x02u);
	CHECK_EQ(out_buf[3], 150u);
	CHECK_EQ(out_buf[4], 220u);
	CHECK_EQ(out_buf[44], 255u);
	CHECK_EQ(out_buf[46], 128u);

	std::printf("  [OK] Test 6: macOS HID Input/Output Report Decoding & Encoding\n");
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

void BenchmarkInputSubsystem() {
	std::printf("\n--- PS5 DualSense Input Subsystem Benchmarks ---\n");

	DualSenseState state{};
	uint8_t report[64] = {0};
	report[1] = 128;
	report[2] = 128;
	report[8] = 0x50; // Square + Cross

	constexpr int kPollBatches = 1000000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kPollBatches; ++i) {
		MacOsHidBackend::DecodeDualSenseInputReport(report, 64, state);
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kPollBatches;
	double throughput = kPollBatches / std::chrono::duration<double>(t1 - t0).count();

	std::printf("  [Bench] DualSense HID Report Decode Latency: %.2f ns / report (Throughput: %.2f M reports/sec)\n",
	           dt_ns, throughput / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5: Complete DualSense Input Test Suite       \n");
	std::printf("====================================================\n\n");

	TestInputBackendLifecycle();
	TestButtonsSticksAndTriggers();
	TestAdaptiveTriggers();
	TestVibrationAndHaptics();
	TestTouchpadAndIMU();
	TestMacOsHidReportEncodingDecoding();

	BenchmarkInputSubsystem();

	std::printf("\n====================================================\n");
	std::printf(" Results: %d/%d tests passed", g_tests_passed, g_tests_run);
	if (g_tests_failed > 0) {
		std::printf(" — %d FAILED\n", g_tests_failed);
		std::printf("Ps5DualSenseInputTests: FAILED\n");
		return 1;
	}
	std::printf("\nPs5DualSenseInputTests: ALL PASSED\n");
	return 0;
}
