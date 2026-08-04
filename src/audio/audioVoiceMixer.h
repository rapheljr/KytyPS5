// audioVoiceMixer.h
//
// Multi-Voice Concurrent PCM Mixer (Up to 64 Voices) for PS5 Audio Subsystem.

#ifndef AUDIO_AUDIO_VOICE_MIXER_H
#define AUDIO_AUDIO_VOICE_MIXER_H

#include "audio/audioEngine.h"
#include "common/common.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Libs::Audio {

constexpr uint32_t kMaxAudioVoices = 64;
constexpr uint32_t kMaxOutputChannels = 12; // 7.1.4 spatial channels

enum class VoiceState : uint8_t {
	Stopped = 0,
	Playing,
	Paused
};

struct AudioVoice {
	uint32_t           voice_id     = 0;
	VoiceState         state        = VoiceState::Stopped;
	uint32_t           sample_rate  = 48000;
	uint32_t           channels     = 2;
	float              volume       = 1.0f;
	float              pitch_ratio  = 1.0f;
	float              panning_matrix[kMaxOutputChannels] = {1.0f, 1.0f};

	std::deque<float>  pcm_ring_buffer;
	mutable std::mutex mutex;
};

class VoiceMixer {
public:
	VoiceMixer() = default;
	~VoiceMixer() = default;

	KYTY_CLASS_NO_COPY(VoiceMixer);

	uint32_t CreateVoice(uint32_t sample_rate = 48000, uint32_t channels = 2);
	bool DestroyVoice(uint32_t voice_id);

	bool PlayVoice(uint32_t voice_id);
	bool PauseVoice(uint32_t voice_id);
	bool StopVoice(uint32_t voice_id);

	bool SetVoiceVolume(uint32_t voice_id, float volume);
	bool SetVoicePanningMatrix(uint32_t voice_id, const float* matrix, uint32_t count);
	bool PushVoiceSamples(uint32_t voice_id, const float* samples, size_t frame_count);

	void MixVoices(float* out_master_pcm, size_t frame_count, uint32_t output_channels);

	[[nodiscard]] size_t GetActiveVoiceCount() const;

private:
	mutable std::mutex                                     m_mutex;
	uint32_t                                               m_next_voice_id = 1;
	std::unordered_map<uint32_t, std::shared_ptr<AudioVoice>> m_voices;
};

} // namespace Libs::Audio

#endif // AUDIO_AUDIO_VOICE_MIXER_H
