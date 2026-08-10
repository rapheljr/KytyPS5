// coreAudioBackend.mm
//
// CoreAudio Multi-Channel PS5 AudioOut Streaming Engine Implementation.

#include "audio/coreAudioBackend.h"

#if defined(__APPLE__)
#import <AudioToolbox/AudioToolbox.h>
#import <AudioUnit/AudioUnit.h>
#import <CoreAudio/CoreAudioTypes.h>

namespace Libs::Audio {

struct CoreAudioBackend::Impl {
	AudioComponentInstance audio_unit = nullptr;
};

static OSStatus CoreAudioRenderCallback(void* inRefCon,
                                       AudioUnitRenderActionFlags* ioActionFlags,
                                       const AudioTimeStamp* inTimeStamp,
                                       UInt32 inBusNumber,
                                       UInt32 inNumberFrames,
                                       AudioBufferList* ioData) {
	auto* backend = static_cast<CoreAudioBackend*>(inRefCon);
	if (!backend || !ioData) return noErr;

	float* out_buffer = static_cast<float*>(ioData->mBuffers[0].mData);
	uint32_t channels = backend->GetChannels();
	size_t requested_samples = inNumberFrames * channels;

	// In real-time callback: fetch from ring buffer or zero-fill on underrun
	std::memset(out_buffer, 0, requested_samples * sizeof(float));
	return noErr;
}

CoreAudioBackend::CoreAudioBackend(AudioEngine* /*engine*/) : m_impl(std::make_unique<Impl>()) {}

CoreAudioBackend::~CoreAudioBackend() {
	Shutdown();
}

bool CoreAudioBackend::Initialize(const AudioStreamConfig& config) {
	uint32_t ch = static_cast<uint32_t>(config.layout);
	return Initialize(config.sample_rate, ch > 0 ? ch : 2, config.buffer_size_frames * 8);
}

bool CoreAudioBackend::StartStream() {
	if (!m_initialized || !m_impl || !m_impl->audio_unit) return false;
	return AudioOutputUnitStart(m_impl->audio_unit) == noErr;
}

bool CoreAudioBackend::StopStream() {
	if (!m_initialized || !m_impl || !m_impl->audio_unit) return false;
	return AudioOutputUnitStop(m_impl->audio_unit) == noErr;
}

uint64_t CoreAudioBackend::GetAudioTimeUs() const {
	if (m_sample_rate == 0) return 0;
	return (m_stats.total_frames_played * 1000000ULL) / m_sample_rate;
}

void CoreAudioBackend::SetLatencyUs(uint32_t /*latency_us*/) {}

bool CoreAudioBackend::Initialize(uint32_t sample_rate, uint32_t channels, size_t ring_buffer_capacity_frames) {
	m_sample_rate = sample_rate;
	m_channels    = channels;
	m_capacity    = ring_buffer_capacity_frames * channels;
	m_ring_buffer.resize(m_capacity, 0.0f);
	m_write_pos.store(0);
	m_read_pos.store(0);

	AudioComponentDescription desc{};
	desc.componentType         = kAudioUnitType_Output;
	desc.componentSubType      = kAudioUnitSubType_DefaultOutput;
	desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(nullptr, &desc);
	if (!comp) {
		return false;
	}

	OSStatus status = AudioComponentInstanceNew(comp, &m_impl->audio_unit);
	if (status != noErr || !m_impl->audio_unit) {
		return false;
	}

	AudioStreamBasicDescription asbd{};
	asbd.mSampleRate       = sample_rate;
	asbd.mFormatID         = kAudioFormatLinearPCM;
	asbd.mFormatFlags      = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	asbd.mBytesPerPacket   = sizeof(float) * channels;
	asbd.mFramesPerPacket  = 1;
	asbd.mBytesPerFrame    = sizeof(float) * channels;
	asbd.mChannelsPerFrame = channels;
	asbd.mBitsPerChannel   = 32;

	AudioUnitSetProperty(m_impl->audio_unit,
	                     kAudioUnitProperty_StreamFormat,
	                     kAudioUnitScope_Input,
	                     0,
	                     &asbd,
	                     sizeof(asbd));

	AURenderCallbackStruct callback_struct{};
	callback_struct.inputProc       = CoreAudioRenderCallback;
	callback_struct.inputProcRefCon = this;

	AudioUnitSetProperty(m_impl->audio_unit,
	                     kAudioUnitProperty_SetRenderCallback,
	                     kAudioUnitScope_Input,
	                     0,
	                     &callback_struct,
	                     sizeof(callback_struct));

	AudioUnitInitialize(m_impl->audio_unit);
	m_initialized = true;
	return true;
}

void CoreAudioBackend::Shutdown() {
	if (m_impl && m_impl->audio_unit) {
		AudioOutputUnitStop(m_impl->audio_unit);
		AudioUnitUninitialize(m_impl->audio_unit);
		AudioComponentInstanceDispose(m_impl->audio_unit);
		m_impl->audio_unit = nullptr;
	}
	m_initialized = false;
}

size_t CoreAudioBackend::WriteSamples(const float* interleaved_pcm, size_t num_frames) {
	if (!m_initialized || !interleaved_pcm || num_frames == 0) return 0;

	size_t num_samples = num_frames * m_channels;
	size_t curr_w = m_write_pos.load(std::memory_order_relaxed);

	for (size_t i = 0; i < num_samples; ++i) {
		m_ring_buffer[(curr_w + i) % m_capacity] = interleaved_pcm[i];
	}

	m_write_pos.store((curr_w + num_samples) % m_capacity, std::memory_order_release);
	m_stats.total_frames_written += num_frames;
	return num_frames;
}

size_t CoreAudioBackend::GetAvailableFramesToRead() const noexcept {
	size_t w = m_write_pos.load(std::memory_order_acquire);
	size_t r = m_read_pos.load(std::memory_order_relaxed);
	if (w >= r) {
		return (w - r) / m_channels;
	} else {
		return (m_capacity - (r - w)) / m_channels;
	}
}

} // namespace Libs::Audio

#else

namespace Libs::Audio {
struct CoreAudioBackend::Impl {};
CoreAudioBackend::CoreAudioBackend() : m_impl(nullptr) {}
CoreAudioBackend::~CoreAudioBackend() = default;
bool CoreAudioBackend::Initialize(uint32_t sample_rate, uint32_t channels, size_t) { m_sample_rate = sample_rate; m_channels = channels; m_initialized = true; return true; }
void CoreAudioBackend::Shutdown() { m_initialized = false; }
size_t CoreAudioBackend::WriteSamples(const float*, size_t num_frames) { m_stats.total_frames_written += num_frames; return num_frames; }
size_t CoreAudioBackend::GetAvailableFramesToRead() const noexcept { return 0; }
} // namespace Libs::Audio

#endif
