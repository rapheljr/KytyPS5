// inputEngine.cpp
//
// Platform-Independent Input Engine & Interface Implementation.

#include "input/inputEngine.h"

#include <chrono>

namespace Libs::Input {

DualSenseController::DualSenseController(uint32_t index) : m_index(index) {}

void DualSenseController::SetState(const DualSenseState& state) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state = state;
}

DualSenseState DualSenseController::GetState() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_state;
}

void DualSenseController::SetButton(DualSenseButtons button, bool pressed) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (pressed) {
		m_state.buttons = m_state.buttons | button;
	} else {
		m_state.buttons = static_cast<DualSenseButtons>(
		    static_cast<uint32_t>(m_state.buttons) & ~static_cast<uint32_t>(button));
	}
}

void DualSenseController::SetAnalogSticks(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.left_stick_x  = lx;
	m_state.left_stick_y  = ly;
	m_state.right_stick_x = rx;
	m_state.right_stick_y = ry;
}

void DualSenseController::SetTriggers(uint8_t l2, uint8_t r2) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.trigger_l2 = l2;
	m_state.trigger_r2 = r2;
}

void DualSenseController::SetAdaptiveTrigger(bool is_r2, const AdaptiveTriggerConfig& config) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (is_r2) {
		m_state.trigger_r2_effect = config;
	} else {
		m_state.trigger_l2_effect = config;
	}
}

void DualSenseController::SetTouch0(bool active, uint16_t x, uint16_t y, uint8_t id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.touch0.active = active;
	m_state.touch0.x      = x;
	m_state.touch0.y      = y;
	m_state.touch0.id     = id;
}

void DualSenseController::SetTouch1(bool active, uint16_t x, uint16_t y, uint8_t id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.touch1.active = active;
	m_state.touch1.x      = x;
	m_state.touch1.y      = y;
	m_state.touch1.id     = id;
}

void DualSenseController::SetIMU(float gyro_x, float gyro_y, float gyro_z, float accel_x, float accel_y, float accel_z) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.imu.gyro_x  = gyro_x;
	m_state.imu.gyro_y  = gyro_y;
	m_state.imu.gyro_z  = gyro_z;
	m_state.imu.accel_x = accel_x;
	m_state.imu.accel_y = accel_y;
	m_state.imu.accel_z = accel_z;
}

void DualSenseController::SetVibration(uint8_t small_motor, uint8_t large_motor) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.haptics.small_motor_rumble = small_motor;
	m_state.haptics.large_motor_rumble = large_motor;
}

void DualSenseController::SetLightbar(uint8_t r, uint8_t g, uint8_t b) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_state.lightbar.r = r;
	m_state.lightbar.g = g;
	m_state.lightbar.b = b;
}

// ─── InputManager ─────────────────────────────────────────────────────────────

InputManager::InputManager() {
	for (uint32_t i = 0; i < kMaxControllers; ++i) {
		m_controllers[i] = std::make_unique<DualSenseController>(i);
	}
}

InputManager::~InputManager() {
	Shutdown();
}

bool InputManager::Initialize(std::unique_ptr<IInputBackend> backend) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (backend) {
		m_backend = std::move(backend);
		return m_backend->Initialize();
	}
	return true;
}

void InputManager::Shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_backend) {
		m_backend->Shutdown();
		m_backend.reset();
	}
}

void InputManager::PollAll() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_backend) {
		for (uint32_t i = 0; i < kMaxControllers; ++i) {
			DualSenseState state;
			if (m_backend->PollInput(i, state) && m_controllers[i]) {
				m_controllers[i]->SetState(state);
			}
		}
	}
}

DualSenseController* InputManager::GetController(uint32_t index) {
	if (index >= kMaxControllers) return nullptr;
	return m_controllers[index].get();
}

} // namespace Libs::Input
