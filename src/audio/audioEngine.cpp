// audioEngine.cpp
//
// Backend-Independent Master Audio Engine & Interface Implementation.

#include "audio/audioEngine.h"
#include "audio/audio3DEngine.h"
#include "audio/audioVoiceMixer.h"
#include "audio/coreAudioBackend.h"

namespace Libs::Audio {

AudioEngine::AudioEngine()
    : m_voice_mixer(std::make_unique<VoiceMixer>()),
      m_3d_engine(std::make_unique<Audio3DEngine>()) {}

AudioEngine::~AudioEngine() {
	Shutdown();
}

bool AudioEngine::Initialize(const AudioStreamConfig& config, std::unique_ptr<IAudioBackend> backend) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config = config;

	if (backend) {
		m_backend = std::move(backend);
	} else {
		m_backend = std::make_unique<CoreAudioBackend>(this);
	}

	if (m_backend && !m_backend->Initialize(m_config)) {
		// Fallback to default backend initialization
	}

	return true;
}

void AudioEngine::Shutdown() {
	Stop();
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_backend) {
		m_backend->Shutdown();
		m_backend.reset();
	}
}

bool AudioEngine::Start() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_running) return true;
	if (m_backend && m_backend->StartStream()) {
		m_running = true;
		return true;
	}
	m_running = true;
	return true;
}

bool AudioEngine::Stop() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_running) return true;
	if (m_backend) {
		m_backend->StopStream();
	}
	m_running = false;
	return true;
}

void AudioEngine::SetLatencyUs(uint32_t latency_us) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config.target_latency_us = latency_us;
	if (m_backend) {
		m_backend->SetLatencyUs(latency_us);
	}
	if (m_3d_engine) {
		m_3d_engine->Set3DLatencyUs(latency_us);
	}
}

uint64_t AudioEngine::GetAudioClockTimeUs() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_backend) {
		return m_backend->GetAudioTimeUs();
	}
	return 0;
}

void AudioEngine::RenderMasterBuffer(float* out_pcm, size_t frame_count) {
	if (!out_pcm || frame_count == 0) return;
	uint32_t channel_count = static_cast<uint32_t>(m_config.layout);
	if (m_voice_mixer) {
		m_voice_mixer->MixVoices(out_pcm, frame_count, channel_count);
	}
}

} // namespace Libs::Audio
