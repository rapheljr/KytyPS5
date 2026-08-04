// coreAudioBackend.h
//
// Apple CoreAudio Hardware Backend for PS5 Audio Subsystem.

#ifndef AUDIO_CORE_AUDIO_BACKEND_H
#define AUDIO_CORE_AUDIO_BACKEND_H

#include "audio/audioEngine.h"
#include "common/common.h"

#include <atomic>
#include <cstdint>

#if defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#endif

namespace Libs::Audio {

class CoreAudioBackend : public IAudioBackend {
public:
	explicit CoreAudioBackend(AudioEngine* engine = nullptr);
	~CoreAudioBackend() override;

	KYTY_CLASS_NO_COPY(CoreAudioBackend);

	bool Initialize(const AudioStreamConfig& config) override;
	void Shutdown() override;
	bool StartStream() override;
	bool StopStream() override;
	uint64_t GetAudioTimeUs() const override;
	void SetLatencyUs(uint32_t latency_us) override;
	const char* GetBackendName() const override { return "Apple CoreAudio HAL"; }

	void RenderAudio(float* buffer, size_t frame_count);

private:
	AudioEngine*       m_engine = nullptr;
	AudioStreamConfig  m_config{};
	std::atomic<bool>  m_running{false};
	std::atomic<uint64_t> m_audio_time_us{0};
	uint32_t           m_latency_us = 10000;

#if defined(__APPLE__)
	AudioComponentInstance m_audio_unit = nullptr;
#endif
};

} // namespace Libs::Audio

#endif // AUDIO_CORE_AUDIO_BACKEND_H
