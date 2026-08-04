// audioVoiceMixer.cpp
//
// Multi-Voice Concurrent PCM Mixer Implementation.

#include "audio/audioVoiceMixer.h"

#include <algorithm>
#include <cstring>

namespace Libs::Audio {

uint32_t VoiceMixer::CreateVoice(uint32_t sample_rate, uint32_t channels) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_voices.size() >= kMaxAudioVoices) {
		return 0; // Max voice limit reached
	}

	uint32_t vid = m_next_voice_id++;
	auto voice = std::make_shared<AudioVoice>();
	voice->voice_id    = vid;
	voice->sample_rate = sample_rate;
	voice->channels    = channels;
	voice->state       = VoiceState::Stopped;

	for (size_t i = 0; i < kMaxOutputChannels; ++i) {
		voice->panning_matrix[i] = (i < channels) ? 1.0f : 0.0f;
	}

	m_voices[vid] = voice;
	return vid;
}

bool VoiceMixer::DestroyVoice(uint32_t voice_id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) {
		return false;
	}
	m_voices.erase(it);
	return true;
}

bool VoiceMixer::PlayVoice(uint32_t voice_id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) return false;
	it->second->state = VoiceState::Playing;
	return true;
}

bool VoiceMixer::PauseVoice(uint32_t voice_id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) return false;
	it->second->state = VoiceState::Paused;
	return true;
}

bool VoiceMixer::StopVoice(uint32_t voice_id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) return false;
	it->second->state = VoiceState::Stopped;
	std::lock_guard<std::mutex> vlock(it->second->mutex);
	it->second->pcm_ring_buffer.clear();
	return true;
}

bool VoiceMixer::SetVoiceVolume(uint32_t voice_id, float volume) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) return false;
	it->second->volume = std::clamp(volume, 0.0f, 1.0f);
	return true;
}

bool VoiceMixer::SetVoicePanningMatrix(uint32_t voice_id, const float* matrix, uint32_t count) {
	if (!matrix || count == 0) return false;
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_voices.find(voice_id);
	if (it == m_voices.end()) return false;

	uint32_t copy_count = std::min(count, kMaxOutputChannels);
	for (uint32_t i = 0; i < copy_count; ++i) {
		it->second->panning_matrix[i] = matrix[i];
	}
	return true;
}

bool VoiceMixer::PushVoiceSamples(uint32_t voice_id, const float* samples, size_t frame_count) {
	if (!samples || frame_count == 0) return false;
	std::shared_ptr<AudioVoice> voice;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_voices.find(voice_id);
		if (it == m_voices.end()) return false;
		voice = it->second;
	}

	std::lock_guard<std::mutex> vlock(voice->mutex);
	size_t sample_count = frame_count * voice->channels;
	for (size_t i = 0; i < sample_count; ++i) {
		voice->pcm_ring_buffer.push_back(samples[i]);
	}
	return true;
}

void VoiceMixer::MixVoices(float* out_master_pcm, size_t frame_count, uint32_t output_channels) {
	if (!out_master_pcm || frame_count == 0 || output_channels == 0) return;
	uint32_t safe_channels = std::min(output_channels, kMaxOutputChannels);
	std::memset(out_master_pcm, 0, frame_count * safe_channels * sizeof(float));

	std::vector<std::shared_ptr<AudioVoice>> active_voices;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (const auto& [id, v] : m_voices) {
			if (v->state == VoiceState::Playing) {
				active_voices.push_back(v);
			}
		}
	}

	for (const auto& voice : active_voices) {
		std::lock_guard<std::mutex> vlock(voice->mutex);
		auto& ring = voice->pcm_ring_buffer;
		uint32_t v_ch = voice->channels;

		for (size_t f = 0; f < frame_count; ++f) {
			if (ring.size() < v_ch) break;

			for (uint32_t ch = 0; ch < safe_channels; ++ch) {
				float src_sample = ring[ch % v_ch];
				float panned_sample = src_sample * voice->volume * voice->panning_matrix[ch];
				out_master_pcm[f * safe_channels + ch] += panned_sample;
			}

			for (uint32_t c = 0; c < v_ch; ++c) {
				ring.pop_front();
			}
		}
	}

	// Clamp output to [-1.0f, 1.0f]
	size_t total_samples = frame_count * safe_channels;
	for (size_t i = 0; i < total_samples; ++i) {
		out_master_pcm[i] = std::clamp(out_master_pcm[i], -1.0f, 1.0f);
	}
}

size_t VoiceMixer::GetActiveVoiceCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	size_t count = 0;
	for (const auto& [id, v] : m_voices) {
		if (v->state == VoiceState::Playing) {
			count++;
		}
	}
	return count;
}

} // namespace Libs::Audio
