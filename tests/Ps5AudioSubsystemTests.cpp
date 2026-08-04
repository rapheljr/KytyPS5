// Ps5AudioSubsystemTests.cpp
//
// Complete Automated Test Suite for PS5 Audio Subsystem, IAudioBackend,
// CoreAudio Backend, VoiceMixer, and Audio3DEngine.

#include "audio/audio3DEngine.h"
#include "audio/audioEngine.h"
#include "audio/audioVoiceMixer.h"
#include "audio/coreAudioBackend.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

void Check(bool value, const char* description, const char* file, int line) {
	g_tests_run++;
	if (!value) {
		g_tests_failed++;
		std::printf("  [FAIL] %s\n         at %s:%d\n", description, file, line);
	} else {
		g_tests_passed++;
	}
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) Check((a) == (b), #a " == " #b, __FILE__, __LINE__)

using namespace Libs::Audio;

// Dummy mock audio backend for isolated testing
class MockAudioBackend : public IAudioBackend {
public:
	MockAudioBackend() = default;
	~MockAudioBackend() override = default;

	bool Initialize(const AudioStreamConfig& config) override {
		m_config = config;
		return true;
	}
	void Shutdown() override { m_running = false; }
	bool StartStream() override { m_running = true; return true; }
	bool StopStream() override { m_running = false; return true; }
	uint64_t GetAudioTimeUs() const override { return m_time_us; }
	void SetLatencyUs(uint32_t latency_us) override { m_latency_us = latency_us; }
	const char* GetBackendName() const override { return "Mock Test Audio Backend"; }

	void AdvanceTime(uint64_t us) { m_time_us += us; }

private:
	AudioStreamConfig m_config{};
	bool              m_running    = false;
	uint64_t          m_time_us    = 0;
	uint32_t          m_latency_us = 10000;
};

// ─── 1. Backend Initialization & Interface Test ──────────────────────────────

void TestAudioBackendInterface() {
	std::printf("  [Test 1] IAudioBackend Interface & Mock/CoreAudio Backend...\n");

	AudioEngine engine;
	auto mock_ptr = std::make_unique<MockAudioBackend>();
	auto* mock_ref = mock_ptr.get();

	AudioStreamConfig config{};
	config.sample_rate = 48000;
	config.layout      = Libs::Audio::AudioChannelLayout::Stereo;

	CHECK(engine.Initialize(config, std::move(mock_ptr)));
	CHECK(engine.Start());
	CHECK(engine.IsRunning());

	CHECK_EQ(std::string(engine.GetBackend()->GetBackendName()), std::string("Mock Test Audio Backend"));

	mock_ref->AdvanceTime(5000);
	CHECK_EQ(engine.GetAudioClockTimeUs(), 5000u);

	engine.SetLatencyUs(20000);
	CHECK_EQ(engine.GetConfig().target_latency_us, 20000u);

	CHECK(engine.Stop());
	std::printf("  [OK] Test 1: IAudioBackend Interface & Mock/CoreAudio Backend\n");
}

// ─── 2. Multi-Voice Mixer Test (64 Voices) ───────────────────────────────────

void TestMultiVoiceMixer() {
	std::printf("  [Test 2] Multi-Voice Concurrent PCM Mixer (Up to 64 Voices)...\n");

	VoiceMixer mixer;
	std::vector<uint32_t> voice_ids;

	for (int i = 0; i < 64; ++i) {
		uint32_t vid = mixer.CreateVoice(48000, 2);
		CHECK(vid > 0);
		voice_ids.push_back(vid);
	}

	// 65th voice creation should fail (limit 64)
	uint32_t overflow_vid = mixer.CreateVoice(48000, 2);
	CHECK_EQ(overflow_vid, 0u);

	// Push stereo PCM samples to voice 1
	std::vector<float> sample_data(1024, 0.5f);
	CHECK(mixer.PushVoiceSamples(voice_ids[0], sample_data.data(), 512));
	CHECK(mixer.PlayVoice(voice_ids[0]));
	CHECK_EQ(mixer.GetActiveVoiceCount(), 1u);

	// Mix frames
	std::vector<float> master_out(1024, 0.0f);
	mixer.MixVoices(master_out.data(), 256, 2);

	// First samples should be mixed to 0.5f
	CHECK(std::abs(master_out[0] - 0.5f) < 0.001f);

	CHECK(mixer.StopVoice(voice_ids[0]));
	CHECK_EQ(mixer.GetActiveVoiceCount(), 0u);

	for (uint32_t vid : voice_ids) {
		mixer.DestroyVoice(vid);
	}

	std::printf("  [OK] Test 2: Multi-Voice Concurrent PCM Mixer\n");
}

// ─── 3. 3D Spatial Audio & VBAP Panning Test ─────────────────────────────────

void Test3DSpatialAudioVBAP() {
	std::printf("  [Test 3] 3D Spatial Audio & VBAP Coefficient Calculation...\n");

	AudioPosition3D pos_right{};
	pos_right.x = 10.0f;  // Pure Right
	pos_right.y = 0.0f;
	pos_right.z = 0.0f;
	pos_right.min_dist = 1.0f;
	pos_right.max_dist = 50.0f;

	float coeffs_right[12] = {0};
	Audio3DEngine::ComputeVbapCoefficients(pos_right, coeffs_right, 12, true);

	// Right channel (index 1) should be greater than Left channel (index 0)
	CHECK(coeffs_right[1] > coeffs_right[0]);

	AudioPosition3D pos_top{};
	pos_top.x = 0.0f;
	pos_top.y = 0.0f;
	pos_top.z = 10.0f; // Top / Elevated

	float coeffs_top[12] = {0};
	Audio3DEngine::ComputeVbapCoefficients(pos_top, coeffs_top, 12, true);

	// Top channels (index 8..11) should have positive coefficient
	CHECK(coeffs_top[8] > 0.0f || coeffs_top[9] > 0.0f);

	// Ambisonics test
	float ambi_coeffs[4] = {0};
	Audio3DEngine::ComputeAmbisonicsCoefficients(1, 0.0f, 0.0f, ambi_coeffs, 4);
	CHECK_EQ(ambi_coeffs[0], 0.70710678f);

	std::printf("  [OK] Test 3: 3D Spatial Audio & VBAP Coefficient Calculation\n");
}

// ─── 4. Apple CoreAudio Hardware Render Callback Test ────────────────────────

void TestCoreAudioRenderCallback() {
	std::printf("  [Test 4] CoreAudio Hardware Backend Render Callback...\n");

	AudioEngine engine;
	AudioStreamConfig config{};
	config.sample_rate = 48000;
	config.layout      = Libs::Audio::AudioChannelLayout::Stereo;

	CoreAudioBackend backend(&engine);
	CHECK(backend.Initialize(config));
	CHECK(backend.StartStream());

	// Simulate CoreAudio HAL buffer render
	std::vector<float> pcm_buf(1024, 0.0f);
	backend.RenderAudio(pcm_buf.data(), 512);

	CHECK(backend.GetAudioTimeUs() > 0);

	CHECK(backend.StopStream());
	backend.Shutdown();

	std::printf("  [OK] Test 4: CoreAudio Hardware Backend Render Callback\n");
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

void BenchmarkAudioSubsystem() {
	std::printf("\n--- PS5 Audio Subsystem Benchmarks ---\n");

	VoiceMixer mixer;
	std::vector<uint32_t> active_voices;

	for (int i = 0; i < 32; ++i) {
		uint32_t vid = mixer.CreateVoice(48000, 2);
		std::vector<float> chunk(4096, 0.2f);
		mixer.PushVoiceSamples(vid, chunk.data(), 2048);
		mixer.PlayVoice(vid);
		active_voices.push_back(vid);
	}

	std::vector<float> master_buf(2048 * 2, 0.0f);
	constexpr int kMixBatches = 10000;

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kMixBatches; ++i) {
		mixer.MixVoices(master_buf.data(), 512, 2);
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kMixBatches;
	double total_frames = kMixBatches * 512;
	double throughput_fps = total_frames / std::chrono::duration<double>(t1 - t0).count();

	std::printf("  [Bench] 32-Voice Mixer Latency (512 frames): %.2f us / batch (Throughput: %.2f M frames/sec)\n",
	           dt_us, throughput_fps / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5: Complete PS5 Audio Subsystem Test Suite   \n");
	std::printf("====================================================\n\n");

	TestAudioBackendInterface();
	TestMultiVoiceMixer();
	Test3DSpatialAudioVBAP();
	TestCoreAudioRenderCallback();

	BenchmarkAudioSubsystem();

	std::printf("\n====================================================\n");
	std::printf(" Results: %d/%d tests passed", g_tests_passed, g_tests_run);
	if (g_tests_failed > 0) {
		std::printf(" — %d FAILED\n", g_tests_failed);
		std::printf("Ps5AudioSubsystemTests: FAILED\n");
		return 1;
	}
	std::printf("\nPs5AudioSubsystemTests: ALL PASSED\n");
	return 0;
}
