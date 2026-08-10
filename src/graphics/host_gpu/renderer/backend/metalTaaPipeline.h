// metalTaaPipeline.h
//
// Metal Temporal Anti-Aliasing (TAA) & Motion Vector Reconstruction Pipeline for KytyPS5.
// Implements sub-pixel Halton jittering, history reprojection, and YCoCg AABB variance clipping.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_TAA_PIPELINE_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_TAA_PIPELINE_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Graphics::HostGpu {

struct JitterOffset {
	float x = 0.0f;
	float y = 0.0f;
};

struct TaaConfig {
	float    blend_weight        = 0.1f; // History blend weight
	uint32_t jitter_sample_count = 8;    // 8-phase Halton sequence
	bool     enable_variance_clipping = true;
};

struct TaaPipelineStats {
	uint64_t total_frames_resolved = 0;
	uint32_t current_jitter_phase  = 0;
};

class MetalTaaPipeline {
public:
	MetalTaaPipeline();
	~MetalTaaPipeline();

	KYTY_CLASS_NO_COPY(MetalTaaPipeline);

	bool Initialize(const TaaConfig& config = {});
	void Shutdown();

	/// Get next subpixel jitter offset for projection matrix
	JitterOffset GetNextJitter();

	/// Resolve current frame with history buffer
	void ResolveFrame(const float* current_rgba, const float* history_rgba, float* out_rgba, size_t pixel_count);

	[[nodiscard]] const TaaPipelineStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

	static float ComputeHalton(uint32_t index, uint32_t base);

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	TaaConfig             m_config{};
	TaaPipelineStats      m_stats{};
	bool                  m_initialized = false;
};

} // namespace Graphics::HostGpu

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_TAA_PIPELINE_H
