// metalVrsPipeline.mm
//
// Metal Variable Rate Shading (VRS) & Dynamic Resolution Scaling (DRS) Implementation.

#include "graphics/host_gpu/renderer/backend/metalVrsPipeline.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <algorithm>
#include <iostream>
#include <mutex>

namespace Graphics::HostGpu {

struct MetalVrsPipeline::Impl {
	id<MTLDevice> device = nil;
	std::mutex    mutex;

	~Impl() {
		device = nil;
	}
};

MetalVrsPipeline::MetalVrsPipeline() : m_impl(std::make_unique<Impl>()) {}
MetalVrsPipeline::~MetalVrsPipeline() { Shutdown(); }

bool MetalVrsPipeline::Initialize(const VrsDrsConfig& config) {
	if (m_initialized) return true;

	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			std::cerr << "[MetalVrsPipeline] No Metal device available\n";
			return false;
		}

		m_config = config;
		m_stats = {};
		m_stats.current_render_scale = config.max_render_scale;
		m_stats.current_shading_rate = config.default_rate;
		m_initialized = true;

		return true;
	}
}

void MetalVrsPipeline::Shutdown() {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->device = nil;
	m_initialized = false;
}

void MetalVrsPipeline::EvaluateFrame(float measured_frame_time_ms) {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_stats.total_frames_evaluated++;

	if (!m_config.enable_drs && !m_config.enable_vrs) return;

	float delta = measured_frame_time_ms - m_config.target_frame_time_ms;

	if (delta > 2.0f) { // Dropping frames, decrease render scale or increase VRS coarse shading
		if (m_config.enable_drs && m_stats.current_render_scale > m_config.min_render_scale) {
			m_stats.current_render_scale = std::max(m_config.min_render_scale, m_stats.current_render_scale - 0.05f);
			m_stats.drs_adjustments_count++;
		}
		if (m_config.enable_vrs) {
			m_stats.current_shading_rate = MetalShadingRate::Rate2x2;
		}
	} else if (delta < -2.0f) { // Headroom available, scale up
		if (m_config.enable_drs && m_stats.current_render_scale < m_config.max_render_scale) {
			m_stats.current_render_scale = std::min(m_config.max_render_scale, m_stats.current_render_scale + 0.05f);
			m_stats.drs_adjustments_count++;
		}
		if (m_config.enable_vrs) {
			m_stats.current_shading_rate = MetalShadingRate::Rate1x1;
		}
	}
}

} // namespace Graphics::HostGpu
