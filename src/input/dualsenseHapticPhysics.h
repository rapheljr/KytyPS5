// dualsenseHapticPhysics.h
//
// DualSense Adaptive Trigger Haptic Physics Simulation Engine.
// Simulates PS5 mechanical trigger motor resistance, pulse curves, and kickback forces.

#ifndef INPUT_DUALSENSE_HAPTIC_PHYSICS_H
#define INPUT_DUALSENSE_HAPTIC_PHYSICS_H

#include "common/common.h"

#include <cstdint>
#include <vector>

namespace Input {

enum class TriggerEffectMode : uint8_t {
	Off = 0,
	Rigid,
	Pulse,
	WeaponKickback,
	Vibration
};

struct TriggerEffectConfig {
	TriggerEffectMode mode          = TriggerEffectMode::Off;
	uint8_t           start_position = 0;   // Trigger threshold (0-255)
	uint8_t           end_position   = 255; // End resistance point (0-255)
	uint8_t           force          = 0;   // Motor resistance force (0-255)
	uint8_t           frequency      = 0;   // Pulse/vibration frequency in Hz
};

struct HapticPhysicsStats {
	uint64_t total_force_evaluations = 0;
	uint32_t active_triggers_count   = 0;
};

class DualSenseHapticPhysicsEngine {
public:
	DualSenseHapticPhysicsEngine() = default;
	~DualSenseHapticPhysicsEngine() = default;

	KYTY_CLASS_NO_COPY(DualSenseHapticPhysicsEngine);

	/// Configure trigger effect profile for left / right trigger
	void SetLeftTriggerEffect(const TriggerEffectConfig& config);
	void SetRightTriggerEffect(const TriggerEffectConfig& config);

	/// Calculate motor resistance force given physical trigger position (0-255)
	uint8_t EvaluateLeftTriggerForce(uint8_t raw_position);
	uint8_t EvaluateRightTriggerForce(uint8_t raw_position);

	[[nodiscard]] const HapticPhysicsStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	uint8_t ComputeForce(const TriggerEffectConfig& cfg, uint8_t raw_position);

	TriggerEffectConfig m_left_config{};
	TriggerEffectConfig m_right_config{};
	HapticPhysicsStats  m_stats{};
};

} // namespace Input

#endif // INPUT_DUALSENSE_HAPTIC_PHYSICS_H
