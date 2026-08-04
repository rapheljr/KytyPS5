// dualSenseState.h
//
// DualSense Controller State Structures (Buttons, Triggers, Adaptive Triggers, Haptics, Touchpad, IMU).

#ifndef INPUT_DUALSENSE_STATE_H
#define INPUT_DUALSENSE_STATE_H

#include "common/common.h"

#include <array>
#include <cstdint>
#include <vector>

namespace Libs::Input {

enum class DualSenseButtons : uint32_t {
	None        = 0,
	DPadUp      = 1 << 0,
	DPadRight   = 1 << 1,
	DPadDown    = 1 << 2,
	DPadLeft    = 1 << 3,
	Square      = 1 << 4,
	Cross       = 1 << 5,
	Circle      = 1 << 6,
	Triangle    = 1 << 7,
	L1          = 1 << 8,
	R1          = 1 << 9,
	L2          = 1 << 10,
	R2          = 1 << 11,
	L3          = 1 << 12,
	R3          = 1 << 13,
	Options     = 1 << 14,
	Create      = 1 << 15, // Share
	PS          = 1 << 16,
	TouchClick  = 1 << 17,
	Mute        = 1 << 18
};

inline DualSenseButtons operator|(DualSenseButtons a, DualSenseButtons b) {
	return static_cast<DualSenseButtons>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(DualSenseButtons a, DualSenseButtons b) {
	return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

enum class AdaptiveTriggerMode : uint8_t {
	Off = 0,
	Feedback,
	Weapon,
	Vibration,
	SlopeFeedback,
	MultiplePosition
};

struct AdaptiveTriggerConfig {
	AdaptiveTriggerMode mode           = AdaptiveTriggerMode::Off;
	uint8_t             start_position = 0;   // 0 .. 255
	uint8_t             force_strength = 0;   // 0 .. 255
	uint8_t             frequency      = 0;   // 0 .. 255
};

struct TouchPoint {
	bool     active = false;
	uint8_t  id     = 0;
	uint16_t x      = 0; // 0 .. 1920
	uint16_t y      = 0; // 0 .. 1080
};

struct IMUData {
	float gyro_x  = 0.0f; // rad/s or deg/s
	float gyro_y  = 0.0f;
	float gyro_z  = 0.0f;
	float accel_x = 0.0f; // g-force
	float accel_y = 0.0f;
	float accel_z = 1.0f; // 1.0g gravity
};

struct HapticsState {
	uint8_t              small_motor_rumble = 0; // 0 .. 255
	uint8_t              large_motor_rumble = 0; // 0 .. 255
	float                haptic_left_amplitude = 0.0f;
	float                haptic_right_amplitude = 0.0f;
	std::vector<float>   haptic_pcm_waveform;
};

struct LightbarColor {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 255; // Default PS5 Blue
};

struct DualSenseState {
	DualSenseButtons      buttons           = DualSenseButtons::None;
	uint8_t               left_stick_x      = 128; // 0..255 (128 = center)
	uint8_t               left_stick_y      = 128;
	uint8_t               right_stick_x     = 128;
	uint8_t               right_stick_y     = 128;
	uint8_t               trigger_l2        = 0;   // 0..255 analog
	uint8_t               trigger_r2        = 0;   // 0..255 analog

	AdaptiveTriggerConfig trigger_l2_effect;
	AdaptiveTriggerConfig trigger_r2_effect;

	TouchPoint            touch0;
	TouchPoint            touch1;

	IMUData               imu;

	HapticsState          haptics;
	LightbarColor         lightbar;

	bool                  connected         = true;
	uint64_t              timestamp_us      = 0;

	std::vector<float>    speaker_pcm_out;
	std::vector<float>    mic_pcm_in;
};

} // namespace Libs::Input

#endif // INPUT_DUALSENSE_STATE_H
