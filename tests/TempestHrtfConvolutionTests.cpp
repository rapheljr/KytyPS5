// TempestHrtfConvolutionTests.cpp
//
// Unit & Integration tests for Tempest 3D HRTF Convolution and Ambisonics.

#include "audio/tempestHrtfConvolutionEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Audio;

void TestAmbisonicEncoding() {
	std::printf("[TEST] Ambisonic B-Format Spatial Source Encoding...\n");

	TempestHrtfConvolutionEngine engine;
	constexpr size_t kSamples = 256;
	std::vector<float> input(kSamples, 1.0f);

	// Sound source directly on Left (+Y)
	Tempest3DSoundSource left_src;
	left_src.x = 0.0f;
	left_src.y = 1.0f;
	left_src.z = 0.0f;
	left_src.gain = 1.0f;

	auto bformat = engine.EncodeBFormat(input.data(), kSamples, left_src);
	Check(bformat.w.size() == kSamples, "W channel size mismatch");
	Check(bformat.y.size() == kSamples, "Y channel size mismatch");

	// In B-format: Y should be positive, X should be ~0, Z should be ~0
	Check(std::abs(bformat.x[0]) < 0.001f, "Front/back channel should be 0 for left source");
	Check(bformat.y[0] > 0.5f, "Left/right channel should be positive for left source");
	Check(std::abs(bformat.z[0]) < 0.001f, "Up/down channel should be 0 for left source");

	std::printf("  [OK] Ambisonic B-Format Spatial Source Encoding\n");
}

void TestBinauralHrtfConvolution() {
	std::printf("[TEST] Binaural HRTF Convolution & Accelerate vDSP...\n");

	TempestHrtfConvolutionEngine engine;
	constexpr size_t kSamples = 512;
	std::vector<float> input(kSamples, 0.0f);

	// Unit impulse at sample 0
	input[0] = 1.0f;

	Tempest3DSoundSource right_src;
	right_src.x = 0.0f;
	right_src.y = -1.0f; // Right ear
	right_src.z = 0.0f;
	right_src.gain = 1.0f;

	std::vector<float> left_out(kSamples, 0.0f);
	std::vector<float> right_out(kSamples, 0.0f);

	engine.ProcessBinaural(input.data(), kSamples, right_src, left_out.data(), right_out.data());

	const auto& stats = engine.GetStats();
	Check(stats.total_frames_processed == kSamples, "Total frames mismatch");
	Check(stats.hrtf_convolutions_computed == 1, "Convolution compute count mismatch");

	// For a source on the right: right ear signal energy should exceed left ear signal energy
	float left_energy = 0.0f;
	float right_energy = 0.0f;
	for (size_t i = 0; i < kSamples; ++i) {
		left_energy += left_out[i] * left_out[i];
		right_energy += right_out[i] * right_out[i];
	}

	Check(right_energy > left_energy, "Right ear energy must be greater for right-side sound source");

	std::printf("  [OK] Binaural HRTF Convolution & Accelerate vDSP\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Tempest 3D HRTF Convolution & Ambisonics Test Suite\n");
	std::printf("================================================================================\n");

	TestAmbisonicEncoding();
	TestBinauralHrtfConvolution();

	std::printf("================================================================================\n");
	std::printf("  Results: All Tempest 3D HRTF Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
