// metalVrsPipeline.h
//
// Metal Variable Rate Shading (VRS) & Dynamic Resolution Scaling (DRS) Pipeline for KytyPS5.
// Adjusts raster shading rates (1x1, 2x1, 2x2) and dynamic render scale targeting fixed 60/120 FPS frame-time budgets.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_VRS_PIPELINE_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_VRS_PIPELINE_H

#include "common/common.h"

#include <cstdint>
#include <memory>

namespace Graphics::HostGpu {

enum class MetalShadingRate : uint8_t {
	Rate1x1 = 0, // Full resolution
	Rate2x1,     // Half horizontal
	Rate1x2,     // Half vertical
	Rate2x2      // Quarter resolution
};

struct VrsDrsConfig {
	bool             enable_vrs           = true;
	bool             enable_drs           = true;
	float            target_frame_time_ms = 16.666f; // 60 FPS target
	float            min_render_scale     = 0.5f;
	float            max_render_scale     = 1.0f;
	MetalShadingRate default_rate         = MetalShadingRate::Rate1x1;
};

struct VrsPipelineStats {
	uint64_t         total_frames_evaluated = 0;
	float            current_render_scale   = 1.0f;
	MetalShadingRate current_shading_rate   = MetalShadingRate::Rate1x1;
	uint32_t         drs_adjustments_count  = 0;
};

class MetalVrsPipeline {
public:
	MetalVrsPipeline();
	~MetalVrsPipeline();

	KYTY_CLASS_NO_COPY(MetalVrsPipeline);

	bool Initialize(const VrsDrsConfig& config = {});
	void Shutdown();

	/// Update frame-time metric and evaluate new render scale / shading rate
	void EvaluateFrame(float measured_frame_time_ms);

	/// Get current recommended viewport render scale
	[[nodiscard]] float GetCurrentRenderScale() const noexcept { return m_stats.current_render_scale; }

	/// Get current recommended raster shading rate
	[[nodiscard]] MetalShadingRate GetCurrentShadingRate() const noexcept { return m_stats.current_shading_rate; }

	[[nodiscard]] const VrsPipelineStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	VrsDrsConfig          m_config{};
	VrsPipelineStats      m_stats{};
	bool                  m_initialized = false;
};

} // namespace Graphics::HostGpu

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_VRS_PIPELINE_H
