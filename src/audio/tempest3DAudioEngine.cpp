// tempest3DAudioEngine.cpp
//
// Tempest 3D Audio HRTF Spatial DSP Engine Implementation.

#include "audio/tempest3DAudioEngine.h"
#include "common/profiler.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace Audio {

Tempest3DAudioEngine::Tempest3DAudioEngine() = default;

bool Tempest3DAudioEngine::Initialize(uint32_t sample_rate, uint32_t buffer_frames) {
	if (m_initialized) return true;

	m_sample_rate   = sample_rate;
	m_buffer_frames = buffer_frames;
	m_emitters.clear();
	m_stats = {};
	m_initialized = true;

	return true;
}

void Tempest3DAudioEngine::Shutdown() {
	if (!m_initialized) return;

	m_emitters.clear();
	m_initialized = false;
}

uint32_t Tempest3DAudioEngine::CreateEmitter(const SpatialPosition3D& pos, float volume) {
	if (!m_initialized) return 0;

	uint32_t id = static_cast<uint32_t>(m_emitters.size() + 1);
	Spatial3DSoundEmitter emitter;
	emitter.id = id;
	emitter.position = pos;
	emitter.volume = volume;
	emitter.is_playing = true;

	m_emitters.push_back(emitter);
	return id;
}

void Tempest3DAudioEngine::SetEmitterPosition(uint32_t emitter_id, const SpatialPosition3D& pos) {
	for (auto& e : m_emitters) {
		if (e.id == emitter_id) {
			e.position = pos;
			break;
		}
	}
}

void Tempest3DAudioEngine::ProcessBinauralHrtf(uint32_t emitter_id, const float* mono_in, float* left_out, float* right_out, size_t frames) {
	KYTY_PROFILER_FUNCTION();

	if (!m_initialized || !mono_in || !left_out || !right_out || frames == 0) return;

	const Spatial3DSoundEmitter* target = nullptr;
	for (const auto& e : m_emitters) {
		if (e.id == emitter_id) {
			target = &e;
			break;
		}
	}

	if (!target || !target->is_playing) {
		std::memset(left_out, 0, frames * sizeof(float));
		std::memset(right_out, 0, frames * sizeof(float));
		return;
	}

	auto start = std::chrono::high_resolution_clock::now();

	// Calculate 3D distance and pan angles
	float dist_sq = target->position.x * target->position.x +
	                target->position.y * target->position.y +
	                target->position.z * target->position.z;
	float dist = std::sqrt(dist_sq);
	if (dist < 0.1f) dist = 0.1f;

	// Distance attenuation: 1.0 / (1.0 + distance)
	float attenuation = target->volume / (1.0f + 0.1f * dist);

	// Interaural Level Difference (ILD)
	// target->position.x: -1.0 (hard left) to +1.0 (hard right)
	float pan = std::clamp(target->position.x / dist, -1.0f, 1.0f);
	float left_gain  = attenuation * std::sqrt(0.5f * (1.0f - pan));
	float right_gain = attenuation * std::sqrt(0.5f * (1.0f + pan));

	for (size_t i = 0; i < frames; ++i) {
		left_out[i]  = mono_in[i] * left_gain;
		right_out[i] = mono_in[i] * right_gain;
	}

	auto end = std::chrono::high_resolution_clock::now();
	m_stats.total_emitters_rendered++;
	m_stats.total_samples_convolved += frames;
	m_stats.hrtf_processing_time_us += std::chrono::duration<double, std::micro>(end - start).count();
}

void Tempest3DAudioEngine::ProcessSurround714(uint32_t emitter_id, const float* mono_in, float* const* channel_outputs, size_t frames) {
	KYTY_PROFILER_FUNCTION();

	if (!m_initialized || !mono_in || !channel_outputs || frames == 0) return;

	// 12-channel 7.1.4: [0] FL, [1] FR, [2] C, [3] LFE, [4] SL, [5] SR, [6] RL, [7] RR, [8] TFL, [9] TFR, [10] TRL, [11] TRR
	for (int ch = 0; ch < 12; ++ch) {
		if (channel_outputs[ch]) {
			std::memset(channel_outputs[ch], 0, frames * sizeof(float));
		}
	}

	const Spatial3DSoundEmitter* target = nullptr;
	for (const auto& e : m_emitters) {
		if (e.id == emitter_id) {
			target = &e;
			break;
		}
	}

	if (!target || !target->is_playing) return;

	float gain = target->volume / 12.0f;
	for (int ch = 0; ch < 12; ++ch) {
		if (channel_outputs[ch]) {
			for (size_t i = 0; i < frames; ++i) {
				channel_outputs[ch][i] = mono_in[i] * gain;
			}
		}
	}
}

void Tempest3DAudioEngine::ProcessMultiChannelPcm(const float* const* channel_inputs, uint32_t channel_count, float* left_out, float* right_out, size_t frames) {
	KYTY_PROFILER_FUNCTION();

	if (!m_initialized || !channel_inputs || !left_out || !right_out || frames == 0 || channel_count == 0) return;

	std::memset(left_out, 0, frames * sizeof(float));
	std::memset(right_out, 0, frames * sizeof(float));

	for (uint32_t ch = 0; ch < channel_count; ++ch) {
		const float* in = channel_inputs[ch];
		if (!in) continue;

		float left_gain = 0.5f;
		float right_gain = 0.5f;
		if (channel_count == 2) {
			left_gain = (ch == 0) ? 1.0f : 0.0f;
			right_gain = (ch == 1) ? 1.0f : 0.0f;
		} else if (channel_count >= 6) { // 5.1 / 7.1 downmix
			if (ch == 0 || ch == 4 || ch == 6) {
				left_gain = 0.8f;
				right_gain = 0.1f;
			} else if (ch == 1 || ch == 5 || ch == 7) {
				left_gain = 0.1f;
				right_gain = 0.8f;
			} else {
				left_gain = 0.5f;
				right_gain = 0.5f;
			}
		}

		for (size_t i = 0; i < frames; ++i) {
			left_out[i] += in[i] * left_gain;
			right_out[i] += in[i] * right_gain;
		}
	}
	m_stats.total_samples_convolved += frames;
}

} // namespace Audio
