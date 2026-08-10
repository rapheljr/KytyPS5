// dualsenseHapticPhysics.cpp
//
// DualSense Adaptive Trigger Haptic Physics Simulation Implementation.

#include "input/dualsenseHapticPhysics.h"

#include <algorithm>
#include <cmath>

namespace Input {

void DualSenseHapticPhysicsEngine::SetLeftTriggerEffect(const TriggerEffectConfig& config) {
	m_left_config = config;
	m_stats.active_triggers_count = (m_left_config.mode != TriggerEffectMode::Off ? 1 : 0) +
	                                (m_right_config.mode != TriggerEffectMode::Off ? 1 : 0);
}

void DualSenseHapticPhysicsEngine::SetRightTriggerEffect(const TriggerEffectConfig& config) {
	m_right_config = config;
	m_stats.active_triggers_count = (m_left_config.mode != TriggerEffectMode::Off ? 1 : 0) +
	                                (m_right_config.mode != TriggerEffectMode::Off ? 1 : 0);
}

uint8_t DualSenseHapticPhysicsEngine::ComputeForce(const TriggerEffectConfig& cfg, uint8_t raw_position) {
	m_stats.total_force_evaluations++;

	if (cfg.mode == TriggerEffectMode::Off) return 0;
	if (raw_position < cfg.start_position) return 0;

	switch (cfg.mode) {
		case TriggerEffectMode::Rigid: {
			if (raw_position >= cfg.start_position) {
				return cfg.force;
			}
			return 0;
		}

		case TriggerEffectMode::Pulse: {
			if (raw_position >= cfg.start_position && raw_position <= cfg.end_position) {
				// Sine wave pulse based on frequency
				float phase = static_cast<float>(raw_position - cfg.start_position) * (cfg.frequency > 0 ? cfg.frequency : 1.0f) * 0.1f;
				float wave = (std::sin(phase) + 1.0f) * 0.5f;
				return static_cast<uint8_t>(cfg.force * wave);
			}
			return 0;
		}

		case TriggerEffectMode::WeaponKickback: {
			// Hard resistance ramp up to start position, then immediate snap drop
			if (raw_position >= cfg.start_position && raw_position < cfg.start_position + 20) {
				return cfg.force;
			}
			return static_cast<uint8_t>(cfg.force / 4);
		}

		case TriggerEffectMode::Vibration: {
			if (raw_position >= cfg.start_position) {
				return (raw_position % 2 == 0) ? cfg.force : 0;
			}
			return 0;
		}

		case TriggerEffectMode::Off:
		default:
			return 0;
	}
}

uint8_t DualSenseHapticPhysicsEngine::EvaluateLeftTriggerForce(uint8_t raw_position) {
	return ComputeForce(m_left_config, raw_position);
}

uint8_t DualSenseHapticPhysicsEngine::EvaluateRightTriggerForce(uint8_t raw_position) {
	return ComputeForce(m_right_config, raw_position);
}

void DualSenseHapticPhysicsEngine::Reset() noexcept {
	m_left_config = {};
	m_right_config = {};
	m_stats = {};
}

} // namespace Input
