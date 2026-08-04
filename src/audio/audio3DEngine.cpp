// audio3DEngine.cpp
//
// 3D Spatial Audio Engine (VBAP / Ambisonics / 7.1.4 Spatial Positioning) Implementation.

#include "audio/audio3DEngine.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Libs::Audio {

constexpr float kPi = 3.14159265358979323846f;

float Audio3DEngine::CalculateDistanceAttenuation(float distance, float min_dist, float max_dist) {
	if (distance <= min_dist) return 1.0f;
	if (distance >= max_dist) return 0.0f;
	return (max_dist - distance) / (max_dist - min_dist);
}

void Audio3DEngine::ComputeVbapCoefficients(const AudioPosition3D& pos, float* out_coefficients, uint32_t channel_count, bool height_aware) {
	if (!out_coefficients || channel_count == 0) return;
	std::memset(out_coefficients, 0, channel_count * sizeof(float));

	float dist = std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
	float atten = CalculateDistanceAttenuation(dist, pos.min_dist, pos.max_dist);

	// Angle calculations
	float azimuth = std::atan2(pos.x, pos.y); // radians: 0 = front, pi/2 = right, -pi/2 = left
	float elevation = (dist > 0.0001f) ? std::asin(std::clamp(pos.z / dist, -1.0f, 1.0f)) : 0.0f;

	// Calculate base 2D panning coefficients (L, R)
	float pan_r = 0.5f * (1.0f + std::sin(azimuth));
	float pan_l = 0.5f * (1.0f - std::sin(azimuth));

	// Calculate front/back panning
	float pan_f = 0.5f * (1.0f + std::cos(azimuth));
	float pan_b = 0.5f * (1.0f - std::cos(azimuth));

	// Calculate height panning
	float pan_h = height_aware ? std::clamp((elevation + kPi / 4.0f) / (kPi / 2.0f), 0.0f, 1.0f) : 0.0f;
	float pan_base = 1.0f - pan_h;

	// 12-channel 7.1.4 layout assignment:
	// 0: L, 1: R, 2: C, 3: LFE, 4: Ls, 5: Rs, 6: Lb, 7: Rb, 8: Tfl, 9: Tfr, 10: Tbl, 11: Tbr
	if (channel_count >= 2) {
		out_coefficients[0] = pan_l * pan_f * pan_base * atten; // L
		out_coefficients[1] = pan_r * pan_f * pan_base * atten; // R
	}
	if (channel_count >= 3) {
		out_coefficients[2] = (1.0f - std::abs(std::sin(azimuth))) * pan_f * pan_base * atten; // Center
	}
	if (channel_count >= 4) {
		out_coefficients[3] = atten * 0.5f; // LFE
	}
	if (channel_count >= 6) {
		out_coefficients[4] = pan_l * pan_b * pan_base * atten; // Ls
		out_coefficients[5] = pan_r * pan_b * pan_base * atten; // Rs
	}
	if (channel_count >= 8) {
		out_coefficients[6] = pan_l * pan_b * 0.7f * pan_base * atten; // Lb
		out_coefficients[7] = pan_r * pan_b * 0.7f * pan_base * atten; // Rb
	}
	if (channel_count >= 12 && height_aware) {
		out_coefficients[8]  = pan_l * pan_f * pan_h * atten; // Tfl
		out_coefficients[9]  = pan_r * pan_f * pan_h * atten; // Tfr
		out_coefficients[10] = pan_l * pan_b * pan_h * atten; // Tbl
		out_coefficients[11] = pan_r * pan_b * pan_h * atten; // Tbr
	}

	// Apply spread (blend towards uniform panning)
	if (pos.spread > 0.0f) {
		float uniform = atten / static_cast<float>(channel_count);
		for (uint32_t c = 0; c < channel_count; ++c) {
			out_coefficients[c] = (1.0f - pos.spread) * out_coefficients[c] + pos.spread * uniform;
		}
	}
}

void Audio3DEngine::ComputeAmbisonicsCoefficients(uint32_t order, float azimuth, float elevation, float* out_coefficients, uint32_t channel_count) {
	if (!out_coefficients || channel_count == 0) return;
	std::memset(out_coefficients, 0, channel_count * sizeof(float));

	// 1st order Ambisonics (B-format): W, X, Y, Z
	out_coefficients[0] = 0.70710678f; // W (Omni)
	if (channel_count >= 2) out_coefficients[1] = std::cos(azimuth) * std::cos(elevation); // X
	if (channel_count >= 3) out_coefficients[2] = std::sin(azimuth) * std::cos(elevation); // Y
	if (channel_count >= 4) out_coefficients[3] = std::sin(elevation);                      // Z
}

bool Audio3DEngine::SetVoicePosition(VoiceMixer& mixer, uint32_t voice_id, const AudioPosition3D& pos, bool height_aware) {
	float coeffs[kMaxOutputChannels] = {0};
	ComputeVbapCoefficients(pos, coeffs, kMaxOutputChannels, height_aware);
	return mixer.SetVoicePanningMatrix(voice_id, coeffs, kMaxOutputChannels);
}

void Audio3DEngine::Set3DLatencyUs(uint32_t latency_us) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_latency_us = latency_us;
}

} // namespace Libs::Audio
