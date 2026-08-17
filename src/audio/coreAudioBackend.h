// coreAudioBackend.h
//
// CoreAudio Multi-Channel PS5 AudioOut Streaming Engine.

#ifndef AUDIO_CORE_AUDIO_BACKEND_H
#define AUDIO_CORE_AUDIO_BACKEND_H

#include "common/common.h"
#include "audio/audioEngine.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Libs::Audio {

struct AudioBackendStats {
	uint64_t total_frames_written = 0;
	uint64_t total_frames_played  = 0;
	uint64_t buffer_underruns     = 0;
};

class CoreAudioBackend : public IAudioBackend {
public:
	explicit CoreAudioBackend(AudioEngine* engine = nullptr);
	~CoreAudioBackend() override;

	KYTY_CLASS_NO_COPY(CoreAudioBackend);

	bool Initialize(const AudioStreamConfig& config) override;
	bool Initialize(uint32_t sample_rate = 48000, uint32_t channels = 2, size_t ring_buffer_capacity_frames = 16384);
	void Shutdown() override;

	bool StartStream() override;
	bool StopStream() override;
	uint64_t GetAudioTimeUs() const override;
	void SetLatencyUs(uint32_t latency_us) override;
	const char* GetBackendName() const override { return "Apple CoreAudio AudioUnit Backend"; }

	/// Push interleaved float PCM audio samples into the ring buffer
	size_t WriteSamples(const float* interleaved_pcm, size_t num_frames);

	/// Pull interleaved float PCM audio samples from the ring buffer into the output buffer
	size_t ReadSamples(float* out_pcm, size_t num_frames);

	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }
	[[nodiscard]] uint32_t GetSampleRate() const noexcept { return m_sample_rate; }
	[[nodiscard]] uint32_t GetChannels() const noexcept { return m_channels; }
	[[nodiscard]] size_t GetAvailableFramesToRead() const noexcept;
	[[nodiscard]] const AudioBackendStats& GetStats() const noexcept { return m_stats; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	bool              m_initialized = false;
	uint32_t          m_sample_rate = 48000;
	uint32_t          m_channels    = 2;
	size_t            m_capacity    = 0;

	std::vector<float>    m_ring_buffer;
	std::atomic<size_t>   m_write_pos{0};
	std::atomic<size_t>   m_read_pos{0};
	mutable AudioBackendStats m_stats{};
};

} // namespace Libs::Audio

#endif // AUDIO_CORE_AUDIO_BACKEND_H
