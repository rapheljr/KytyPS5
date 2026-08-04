// coreAudioBackend.mm
//
// Apple CoreAudio Hardware Backend Implementation for PS5 Audio Subsystem.

#include "audio/coreAudioBackend.h"

#include <chrono>
#include <cstring>
#include <thread>

namespace Libs::Audio {

#if defined(__APPLE__)
static OSStatus CoreAudioRenderCallback(
    void*                        inRefCon,
    AudioUnitRenderActionFlags*  /*ioActionFlags*/,
    const AudioTimeStamp*        /*inTimeStamp*/,
    UInt32                       /*inBusNumber*/,
    UInt32                       inNumberFrames,
    AudioBufferList*             ioData) {
	if (!inRefCon || !ioData) {
		return noErr;
	}

	auto* backend = static_cast<CoreAudioBackend*>(inRefCon);
	if (ioData->mNumberBuffers > 0 && ioData->mBuffers[0].mData) {
		float* out_pcm = static_cast<float*>(ioData->mBuffers[0].mData);
		backend->RenderAudio(out_pcm, inNumberFrames);
	}

	return noErr;
}
#endif

CoreAudioBackend::CoreAudioBackend(AudioEngine* engine) : m_engine(engine) {}

CoreAudioBackend::~CoreAudioBackend() {
	Shutdown();
}

bool CoreAudioBackend::Initialize(const AudioStreamConfig& config) {
	m_config = config;

#if defined(__APPLE__)
	AudioComponentDescription desc{};
	desc.componentType         = kAudioUnitType_Output;
	desc.componentSubType      = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (!comp) {
		return false;
	}

	OSStatus status = AudioComponentInstanceNew(comp, &m_audio_unit);
	if (status != noErr || !m_audio_unit) {
		return false;
	}

	uint32_t channel_count = static_cast<uint32_t>(m_config.layout);
	AudioStreamBasicDescription format{};
	format.mSampleRate       = static_cast<Float64>(m_config.sample_rate);
	format.mFormatID         = kAudioFormatLinearPCM;
	format.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	format.mBytesPerPacket   = channel_count * sizeof(float);
	format.mFramesPerPacket  = 1;
	format.mBytesPerFrame    = channel_count * sizeof(float);
	format.mChannelsPerFrame = channel_count;
	format.mBitsPerChannel   = 32;

	status = AudioUnitSetProperty(
	    m_audio_unit,
	    kAudioUnitProperty_StreamFormat,
	    kAudioUnitScope_Input,
	    0,
	    &format,
	    sizeof(format));

	if (status != noErr) {
		AudioComponentInstanceDispose(m_audio_unit);
		m_audio_unit = nullptr;
		return false;
	}

	AURenderCallbackStruct callback{};
	callback.inputProc       = CoreAudioRenderCallback;
	callback.inputProcRefCon = this;

	status = AudioUnitSetProperty(
	    m_audio_unit,
	    kAudioUnitProperty_SetRenderCallback,
	    kAudioUnitScope_Input,
	    0,
	    &callback,
	    sizeof(callback));

	if (status != noErr) {
		AudioComponentInstanceDispose(m_audio_unit);
		m_audio_unit = nullptr;
		return false;
	}

	AudioUnitInitialize(m_audio_unit);
#endif

	return true;
}

void CoreAudioBackend::Shutdown() {
	StopStream();
#if defined(__APPLE__)
	if (m_audio_unit) {
		AudioUnitUninitialize(m_audio_unit);
		AudioComponentInstanceDispose(m_audio_unit);
		m_audio_unit = nullptr;
	}
#endif
}

bool CoreAudioBackend::StartStream() {
	if (m_running) return true;
	m_running = true;

#if defined(__APPLE__)
	if (m_audio_unit) {
		AudioOutputUnitStart(m_audio_unit);
	}
#endif
	return true;
}

bool CoreAudioBackend::StopStream() {
	if (!m_running) return true;
	m_running = false;

#if defined(__APPLE__)
	if (m_audio_unit) {
		AudioOutputUnitStop(m_audio_unit);
	}
#endif
	return true;
}

uint64_t CoreAudioBackend::GetAudioTimeUs() const {
	return m_audio_time_us.load(std::memory_order_relaxed);
}

void CoreAudioBackend::SetLatencyUs(uint32_t latency_us) {
	m_latency_us = latency_us;
}

void CoreAudioBackend::RenderAudio(float* buffer, size_t frame_count) {
	if (!buffer || frame_count == 0) return;
	uint32_t channels = static_cast<uint32_t>(m_config.layout);

	if (m_engine) {
		m_engine->RenderMasterBuffer(buffer, frame_count);
	} else {
		std::memset(buffer, 0, frame_count * channels * sizeof(float));
	}

	uint64_t elapsed_us = (frame_count * 1'000'000ULL) / m_config.sample_rate;
	m_audio_time_us.fetch_add(elapsed_us, std::memory_order_relaxed);
}

} // namespace Libs::Audio
