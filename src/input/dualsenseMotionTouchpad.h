// dualsenseMotionTouchpad.h
//
// DualSense Capacitive Touchpad & 6-Axis Motion Sensor (IMU) Subsystem for KytyPS5.
// Simulates 2-finger multi-touch tracking and 6-axis Gyro/Accel with deadzone calibration.

#ifndef INPUT_DUALSENSE_MOTION_TOUCHPAD_H
#define INPUT_DUALSENSE_MOTION_TOUCHPAD_H

#include "common/common.h"

#include <cstdint>
#include <mutex>

namespace Input {

struct TouchPoint {
	uint16_t x         = 0;     // 0..1919
	uint16_t y         = 0;     // 0..1079
	bool     is_active = false;
	uint8_t  touch_id  = 0;
};

struct MotionSensorData {
	float gyro_pitch_deg_s = 0.0f;
	float gyro_yaw_deg_s   = 0.0f;
	float gyro_roll_deg_s  = 0.0f;
	float accel_x_g        = 0.0f;
	float accel_y_g        = 0.0f;
	float accel_z_g        = 1.0f; // 1G gravity standard
};

struct TouchpadMotionStats {
	uint64_t total_touch_samples  = 0;
	uint64_t total_motion_samples = 0;
};

class DualSenseMotionTouchpad {
public:
	DualSenseMotionTouchpad();
	~DualSenseMotionTouchpad() = default;

	KYTY_CLASS_NO_COPY(DualSenseMotionTouchpad);

	/// Set touch contact points (up to 2 fingers)
	void SetTouch(size_t index, uint16_t x, uint16_t y, bool active, uint8_t id = 0);

	/// Set 6-axis IMU angular velocities and accelerations
	void SetMotion(const MotionSensorData& motion);

	[[nodiscard]] TouchPoint GetTouch(size_t index) const noexcept;
	[[nodiscard]] MotionSensorData GetMotion() const noexcept;

	[[nodiscard]] const TouchpadMotionStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	TouchPoint          m_touches[2]{};
	MotionSensorData    m_motion{};
	mutable std::mutex  m_mutex;
	TouchpadMotionStats m_stats{};
};

} // namespace Input

#endif // INPUT_DUALSENSE_MOTION_TOUCHPAD_H
