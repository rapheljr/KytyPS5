// dualsenseMotionTouchpad.cpp
//
// DualSense Capacitive Touchpad & 6-Axis Motion Sensor Implementation.

#include "input/dualsenseMotionTouchpad.h"

#include <algorithm>

namespace Input {

DualSenseMotionTouchpad::DualSenseMotionTouchpad() = default;

void DualSenseMotionTouchpad::SetTouch(size_t index, uint16_t x, uint16_t y, bool active, uint8_t id) {
	if (index >= 2) return;

	std::lock_guard<std::mutex> lock(m_mutex);

	m_touches[index].x         = std::min<uint16_t>(x, 1919);
	m_touches[index].y         = std::min<uint16_t>(y, 1079);
	m_touches[index].is_active = active;
	m_touches[index].touch_id  = id;

	m_stats.total_touch_samples++;
}

void DualSenseMotionTouchpad::SetMotion(const MotionSensorData& motion) {
	std::lock_guard<std::mutex> lock(m_mutex);

	m_motion = motion;
	m_stats.total_motion_samples++;
}

TouchPoint DualSenseMotionTouchpad::GetTouch(size_t index) const noexcept {
	if (index >= 2) return {};

	std::lock_guard<std::mutex> lock(m_mutex);
	return m_touches[index];
}

MotionSensorData DualSenseMotionTouchpad::GetMotion() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_motion;
}

void DualSenseMotionTouchpad::Reset() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_touches[0] = {};
	m_touches[1] = {};
	m_motion     = {};
	m_stats      = {};
}

} // namespace Input
