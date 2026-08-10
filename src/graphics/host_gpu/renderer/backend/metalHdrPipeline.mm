// metalHdrPipeline.mm
//
// Metal HDR Display & Tone-Mapping Color Pipeline Implementation.

#include "graphics/host_gpu/renderer/backend/metalHdrPipeline.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <mutex>

namespace Graphics::HostGpu {

struct MetalHdrPipeline::Impl {
	id<MTLDevice> device = nil;
	std::mutex    mutex;

	~Impl() {
		device = nil;
	}
};

MetalHdrPipeline::MetalHdrPipeline() : m_impl(std::make_unique<Impl>()) {}
MetalHdrPipeline::~MetalHdrPipeline() { Shutdown(); }

bool MetalHdrPipeline::Initialize(const HdrDisplayConfig& config) {
	if (m_initialized) return true;

	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			std::cerr << "[MetalHdrPipeline] No default Metal device available\n";
			return false;
		}

		m_config = config;
		m_stats = {};
		m_stats.hdr_display_active = (config.target_space == HdrColorSpace::Apple_ExtendedLinearEDR);
		m_initialized = true;

		return true;
	}
}

void MetalHdrPipeline::Shutdown() {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->device = nil;
	m_initialized = false;
}

void MetalHdrPipeline::SetConfig(const HdrDisplayConfig& config) {
	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_config = config;
	m_stats.hdr_display_active = (config.target_space == HdrColorSpace::Apple_ExtendedLinearEDR);
}

static inline float AcesFilmicToneMap(float x) {
	// Narkowicz 2015 ACES approximation
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;
	return std::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

void MetalHdrPipeline::ProcessFrame(const float* in_rgba, float* out_rgba, size_t pixel_count) {
	if (!m_initialized || !in_rgba || !out_rgba || pixel_count == 0) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	float max_lum = 0.0f;

	for (size_t i = 0; i < pixel_count; ++i) {
		size_t idx = i * 4;
		float r = in_rgba[idx + 0] * m_config.exposure;
		float g = in_rgba[idx + 1] * m_config.exposure;
		float b = in_rgba[idx + 2] * m_config.exposure;
		float a = in_rgba[idx + 3];

		float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
		if (lum > max_lum) max_lum = lum;

		if (m_config.tone_mapper == ToneMappingOperator::ACESFilmic) {
			out_rgba[idx + 0] = AcesFilmicToneMap(r);
			out_rgba[idx + 1] = AcesFilmicToneMap(g);
			out_rgba[idx + 2] = AcesFilmicToneMap(b);
		} else if (m_config.tone_mapper == ToneMappingOperator::Reinhard) {
			out_rgba[idx + 0] = r / (1.0f + r);
			out_rgba[idx + 1] = g / (1.0f + g);
			out_rgba[idx + 2] = b / (1.0f + b);
		} else {
			out_rgba[idx + 0] = r;
			out_rgba[idx + 1] = g;
			out_rgba[idx + 2] = b;
		}

		out_rgba[idx + 3] = a;
	}

	m_stats.total_frames_processed++;
	m_stats.current_peak_luminance = max_lum * m_config.paper_white_nits;
}

} // namespace Graphics::HostGpu
