// metalTaaPipeline.mm
//
// Metal Temporal Anti-Aliasing (TAA) Implementation.

#include "graphics/host_gpu/renderer/backend/metalTaaPipeline.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <iostream>
#include <mutex>

namespace Graphics::HostGpu {

struct MetalTaaPipeline::Impl {
	id<MTLDevice> device = nil;
	std::mutex    mutex;

	~Impl() {
		device = nil;
	}
};

MetalTaaPipeline::MetalTaaPipeline() : m_impl(std::make_unique<Impl>()) {}
MetalTaaPipeline::~MetalTaaPipeline() { Shutdown(); }

bool MetalTaaPipeline::Initialize(const TaaConfig& config) {
	if (m_initialized) return true;

	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			std::cerr << "[MetalTaaPipeline] No Metal device available\n";
			return false;
		}

		m_config = config;
		m_stats = {};
		m_initialized = true;

		return true;
	}
}

void MetalTaaPipeline::Shutdown() {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->device = nil;
	m_initialized = false;
}

float MetalTaaPipeline::ComputeHalton(uint32_t index, uint32_t base) {
	float f = 1.0f;
	float r = 0.0f;
	while (index > 0) {
		f /= static_cast<float>(base);
		r += f * static_cast<float>(index % base);
		index /= base;
	}
	return r;
}

JitterOffset MetalTaaPipeline::GetNextJitter() {
	std::lock_guard<std::mutex> lock(m_impl->mutex);

	uint32_t idx = (m_stats.current_jitter_phase % m_config.jitter_sample_count) + 1;
	m_stats.current_jitter_phase++;

	JitterOffset offset;
	offset.x = ComputeHalton(idx, 2) - 0.5f;
	offset.y = ComputeHalton(idx, 3) - 0.5f;

	return offset;
}

void MetalTaaPipeline::ResolveFrame(const float* current_rgba, const float* history_rgba, float* out_rgba, size_t pixel_count) {
	if (!m_initialized || !current_rgba || !out_rgba || pixel_count == 0) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	float alpha = m_config.blend_weight;

	for (size_t i = 0; i < pixel_count; ++i) {
		size_t idx = i * 4;
		if (history_rgba) {
			out_rgba[idx + 0] = current_rgba[idx + 0] * alpha + history_rgba[idx + 0] * (1.0f - alpha);
			out_rgba[idx + 1] = current_rgba[idx + 1] * alpha + history_rgba[idx + 1] * (1.0f - alpha);
			out_rgba[idx + 2] = current_rgba[idx + 2] * alpha + history_rgba[idx + 2] * (1.0f - alpha);
			out_rgba[idx + 3] = current_rgba[idx + 3];
		} else {
			out_rgba[idx + 0] = current_rgba[idx + 0];
			out_rgba[idx + 1] = current_rgba[idx + 1];
			out_rgba[idx + 2] = current_rgba[idx + 2];
			out_rgba[idx + 3] = current_rgba[idx + 3];
		}
	}

	m_stats.total_frames_resolved++;
}

} // namespace Graphics::HostGpu
