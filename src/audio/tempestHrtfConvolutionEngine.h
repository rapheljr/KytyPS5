// tempestHrtfConvolutionEngine.h
//
// Tempest 3D Audio HRTF Convolution & Ambisonics Engine for Apple Silicon macOS.

#ifndef AUDIO_TEMPEST_HRTF_CONVOLUTION_ENGINE_H
#define AUDIO_TEMPEST_HRTF_CONVOLUTION_ENGINE_H

#include "common/common.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Audio {

struct Tempest3DSoundSource {
	float x = 0.0f; // Front (+) / Back (-)
	float y = 0.0f; // Left (+) / Right (-)
	float z = 0.0f; // Up (+) / Down (-)
	float gain = 1.0f;
};

struct AmbisonicBFormat {
	std::vector<float> w; // Omnidirectional (W)
	std::vector<float> x; // Front/Back (X)
	std::vector<float> y; // Left/Right (Y)
	std::vector<float> z; // Up/Down (Z)
};

struct TempestHrtfStats {
	uint64_t total_frames_processed = 0;
	uint32_t active_sources = 0;
	uint32_t hrtf_convolutions_computed = 0;
};

class TempestHrtfConvolutionEngine {
public:
	static constexpr size_t kDefaultHrirLength = 64;
	static constexpr uint32_t kDefaultSampleRate = 48000;

	TempestHrtfConvolutionEngine();
	~TempestHrtfConvolutionEngine() = default;

	KYTY_CLASS_NO_COPY(TempestHrtfConvolutionEngine);

	// Encode a mono audio buffer and 3D position into 1st-order Ambisonic B-format
	AmbisonicBFormat EncodeBFormat(const float* mono_in, size_t num_samples, const Tempest3DSoundSource& source);

	// Convolve source with HRTF impulse responses and render stereo binaural audio (left, right)
	void ProcessBinaural(const float* mono_in, size_t num_samples, const Tempest3DSoundSource& source,
	                     float* left_out, float* right_out);

	// Generate synthetic HRIR for specific 3D source vector
	void GenerateHrir(const Tempest3DSoundSource& source, std::vector<float>& left_hrir, std::vector<float>& right_hrir);

	const TempestHrtfStats& GetStats() const noexcept { return m_stats; }

private:
	TempestHrtfStats m_stats;
	std::mutex       m_mutex;
};

} // namespace Audio

#endif // AUDIO_TEMPEST_HRTF_CONVOLUTION_ENGINE_H
