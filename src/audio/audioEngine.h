// audioEngine.h
//
// Backend-Independent Master Audio Engine & Interface for PS5 Audio Subsystem.

#ifndef AUDIO_AUDIO_ENGINE_H
#define AUDIO_AUDIO_ENGINE_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace Libs::Audio {

enum class AudioSampleFormat : uint8_t {
	Int16 = 0,
	Int24,
	Float32
};

enum class AudioChannelLayout : uint8_t {
	Mono = 1,
	Stereo = 2,
	Surround51 = 6,
	Surround71 = 8,
	Audio3D_714 = 12 // 7.1.4 (12 channels)
};

struct AudioStreamConfig {
	uint32_t           sample_rate        = 48000;
	AudioChannelLayout layout             = AudioChannelLayout::Stereo;
	AudioSampleFormat  format             = AudioSampleFormat::Float32;
	uint32_t           buffer_size_frames = 512;
	uint32_t           target_latency_us  = 10000; // 10ms default
};

class IAudioBackend {
public:
	virtual ~IAudioBackend() = default;

	virtual bool Initialize(const AudioStreamConfig& config) = 0;
	virtual void Shutdown() = 0;
	virtual bool StartStream() = 0;
	virtual bool StopStream() = 0;
	virtual uint64_t GetAudioTimeUs() const = 0;
	virtual void SetLatencyUs(uint32_t latency_us) = 0;
	virtual const char* GetBackendName() const = 0;
};

class VoiceMixer;
class Audio3DEngine;

class AudioEngine {
public:
	AudioEngine();
	~AudioEngine();

	KYTY_CLASS_NO_COPY(AudioEngine);

	bool Initialize(const AudioStreamConfig& config = AudioStreamConfig{}, std::unique_ptr<IAudioBackend> backend = nullptr);
	void Shutdown();

	bool Start();
	bool Stop();

	void SetLatencyUs(uint32_t latency_us);
	[[nodiscard]] uint64_t GetAudioClockTimeUs() const;

	void RenderMasterBuffer(float* out_pcm, size_t frame_count);

	[[nodiscard]] VoiceMixer& GetVoiceMixer() noexcept { return *m_voice_mixer; }
	[[nodiscard]] Audio3DEngine& Get3DEngine() noexcept { return *m_3d_engine; }
	[[nodiscard]] IAudioBackend* GetBackend() noexcept { return m_backend.get(); }
	[[nodiscard]] const AudioStreamConfig& GetConfig() const noexcept { return m_config; }
	[[nodiscard]] bool IsRunning() const noexcept { return m_running; }

private:
	AudioStreamConfig               m_config{};
	bool                            m_running = false;
	std::unique_ptr<IAudioBackend>  m_backend;
	std::unique_ptr<VoiceMixer>     m_voice_mixer;
	std::unique_ptr<Audio3DEngine>  m_3d_engine;
	mutable std::mutex              m_mutex;
};

} // namespace Libs::Audio

#endif // AUDIO_AUDIO_ENGINE_H
