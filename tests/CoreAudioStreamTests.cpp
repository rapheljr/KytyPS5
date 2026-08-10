// CoreAudioStreamTests.cpp
//
// Unit & Integration Tests for CoreAudio Multi-Channel PS5 AudioOut Streaming Engine.

#include "audio/coreAudioBackend.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <vector>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Audio;

void TestCoreAudioBackendLifecycle() {
	std::printf("[TEST] CoreAudioBackend_Lifecycle\n");

	CoreAudioBackend backend;
	bool ok = backend.Initialize(48000, 2, 4096);
	Check(ok, "CoreAudioBackend Initialize failed");
	Check(backend.IsInitialized(), "Backend should be initialized");
	Check(backend.GetSampleRate() == 48000, "Sample rate mismatch");
	Check(backend.GetChannels() == 2, "Channel count mismatch");

	backend.Shutdown();
	Check(!backend.IsInitialized(), "Backend should be shutdown");

	std::printf("  [ OK ] CoreAudioBackend_Lifecycle\n");
}

void TestCoreAudioSampleStreaming() {
	std::printf("[TEST] CoreAudioBackend_SampleStreaming\n");

	CoreAudioBackend backend;
	bool ok = backend.Initialize(48000, 2, 8192);
	Check(ok, "CoreAudioBackend Initialize failed");

	// Generate a 440 Hz Sine wave stereo stream (480 frames = 10ms of audio)
	std::vector<float> sine_wave(480 * 2, 0.0f);
	for (size_t i = 0; i < 480; ++i) {
		float t = static_cast<float>(i) / 48000.0f;
		float sample = std::sin(2.0f * 3.14159265f * 440.0f * t);
		sine_wave[i * 2 + 0] = sample; // Left
		sine_wave[i * 2 + 1] = sample; // Right
	}

	size_t written = backend.WriteSamples(sine_wave.data(), 480);
	Check(written == 480, "Expected 480 frames written");
	Check(backend.GetStats().total_frames_written == 480, "Stats total_frames_written mismatch");
	Check(backend.GetAvailableFramesToRead() == 480, "Available frames to read mismatch");

	// Write second buffer
	written = backend.WriteSamples(sine_wave.data(), 480);
	Check(written == 480, "Expected second 480 frames written");
	Check(backend.GetStats().total_frames_written == 960, "Stats total_frames_written mismatch");
	Check(backend.GetAvailableFramesToRead() == 960, "Available frames to read mismatch");

	backend.Shutdown();
	std::printf("  [ OK ] CoreAudioBackend_SampleStreaming\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — CoreAudio Multi-Channel PS5 AudioOut Streaming Test Suite\n");
	std::printf("================================================================================\n");

	TestCoreAudioBackendLifecycle();
	TestCoreAudioSampleStreaming();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
