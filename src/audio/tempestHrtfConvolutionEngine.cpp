// tempestHrtfConvolutionEngine.cpp
//
// Tempest 3D Audio HRTF Convolution & Ambisonics Implementation.

#include "audio/tempestHrtfConvolutionEngine.h"

#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Audio {

TempestHrtfConvolutionEngine::TempestHrtfConvolutionEngine() {
	m_stats = {};
}

AmbisonicBFormat TempestHrtfConvolutionEngine::EncodeBFormat(const float* mono_in, size_t num_samples, const Tempest3DSoundSource& source) {
	std::lock_guard<std::mutex> lock(m_mutex);

	AmbisonicBFormat bformat;
	bformat.w.resize(num_samples);
	bformat.x.resize(num_samples);
	bformat.y.resize(num_samples);
	bformat.z.resize(num_samples);

	float distance = std::sqrt(source.x * source.x + source.y * source.y + source.z * source.z);
	if (distance < 0.001f) distance = 0.001f;

	float norm_x = source.x / distance;
	float norm_y = source.y / distance;
	float norm_z = source.z / distance;

	float dist_gain = source.gain / std::max(1.0f, distance);
	constexpr float kInvSqrt2 = 0.70710678f;

	float w_gain = kInvSqrt2 * dist_gain;
	float x_gain = norm_x * dist_gain;
	float y_gain = norm_y * dist_gain;
	float z_gain = norm_z * dist_gain;

	for (size_t i = 0; i < num_samples; ++i) {
		float in_sample = mono_in ? mono_in[i] : 0.0f;
		bformat.w[i] = in_sample * w_gain;
		bformat.x[i] = in_sample * x_gain;
		bformat.y[i] = in_sample * y_gain;
		bformat.z[i] = in_sample * z_gain;
	}

	return bformat;
}

void TempestHrtfConvolutionEngine::GenerateHrir(const Tempest3DSoundSource& source, std::vector<float>& left_hrir, std::vector<float>& right_hrir) {
	left_hrir.assign(kDefaultHrirLength, 0.0f);
	right_hrir.assign(kDefaultHrirLength, 0.0f);

	float distance = std::sqrt(source.x * source.x + source.y * source.y + source.z * source.z);
	if (distance < 0.001f) distance = 0.001f;

	float norm_x = source.x / distance; // Front (+) / Back (-)
	float norm_y = source.y / distance; // Left (+) / Right (-)
	float norm_z = source.z / distance; // Up (+) / Down (-)

	// Interaural Time Difference (ITD) in sample delays:
	// Ear distance ~0.18m => max ITD ~0.7ms => ~34 samples at 48kHz
	float itd_samples = norm_y * 16.0f; // [-16, +16]
	int left_delay = std::clamp(static_cast<int>(16.0f - itd_samples), 0, static_cast<int>(kDefaultHrirLength - 8));
	int right_delay = std::clamp(static_cast<int>(16.0f + itd_samples), 0, static_cast<int>(kDefaultHrirLength - 8));

	// Interaural Level Difference (ILD)
	float left_amp = std::clamp(0.5f + 0.5f * norm_y + 0.1f * norm_x + 0.05f * norm_z, 0.1f, 1.0f);
	float right_amp = std::clamp(0.5f - 0.5f * norm_y + 0.1f * norm_x + 0.05f * norm_z, 0.1f, 1.0f);

	// Direct sound impulse
	left_hrir[left_delay] = left_amp;
	right_hrir[right_delay] = right_amp;

	// Pinna / head spectral reflection tap (subtle high-frequency coloring)
	if (left_delay + 4 < static_cast<int>(kDefaultHrirLength)) {
		left_hrir[left_delay + 4] = -0.2f * left_amp * (1.0f + norm_z);
	}
	if (right_delay + 4 < static_cast<int>(kDefaultHrirLength)) {
		right_hrir[right_delay + 4] = -0.2f * right_amp * (1.0f + norm_z);
	}
}

void TempestHrtfConvolutionEngine::ProcessBinaural(const float* mono_in, size_t num_samples, const Tempest3DSoundSource& source,
                                                   float* left_out, float* right_out) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!mono_in || !left_out || !right_out || num_samples == 0) return;

	std::vector<float> left_hrir;
	std::vector<float> right_hrir;
	GenerateHrir(source, left_hrir, right_hrir);

	// Zero outputs
	std::memset(left_out, 0, num_samples * sizeof(float));
	std::memset(right_out, 0, num_samples * sizeof(float));

	size_t hrir_len = kDefaultHrirLength;

	// Standard causal time-domain FIR HRTF convolution
	for (size_t i = 0; i < num_samples; ++i) {
		float l_sum = 0.0f;
		float r_sum = 0.0f;
		for (size_t k = 0; k < hrir_len && k <= i; ++k) {
			l_sum += mono_in[i - k] * left_hrir[k];
			r_sum += mono_in[i - k] * right_hrir[k];
		}
		left_out[i] = l_sum;
		right_out[i] = r_sum;
	}

	m_stats.total_frames_processed += num_samples;
	m_stats.hrtf_convolutions_computed++;
	m_stats.active_sources = 1;
}

} // namespace Audio
