// metalHdrPipeline.h
//
// Metal HDR Display & Tone-Mapping Color Pipeline for Apple Silicon Liquid Retina XDR.
// Converts PS5 HDR10 (Rec.2020 PQ) to Apple Extended Dynamic Range (EDR) with ACES Filmic tone-mapping.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_HDR_PIPELINE_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_HDR_PIPELINE_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Graphics::HostGpu {

enum class HdrColorSpace : uint8_t {
	SDR_sRGB = 0,
	HDR10_PQ_Rec2020,
	HLG_BT2100,
	Apple_ExtendedLinearEDR
};

enum class ToneMappingOperator : uint8_t {
	Passthrough = 0,
	Reinhard,
	ACESFilmic,
	HableUC2
};

struct HdrDisplayConfig {
	HdrColorSpace       source_space   = HdrColorSpace::HDR10_PQ_Rec2020;
	HdrColorSpace       target_space   = HdrColorSpace::Apple_ExtendedLinearEDR;
	ToneMappingOperator tone_mapper    = ToneMappingOperator::ACESFilmic;
	float               max_peak_nits  = 1000.0f; // Liquid Retina XDR peak
	float               paper_white_nits = 200.0f;
	float               exposure       = 1.0f;
};

struct HdrPipelineStats {
	uint64_t total_frames_processed = 0;
	float    current_peak_luminance = 0.0f;
	bool     hdr_display_active     = false;
};

class MetalHdrPipeline {
public:
	MetalHdrPipeline();
	~MetalHdrPipeline();

	KYTY_CLASS_NO_COPY(MetalHdrPipeline);

	bool Initialize(const HdrDisplayConfig& config = {});
	void Shutdown();

	/// Apply HDR tone mapping to a color frame buffer
	void ProcessFrame(const float* in_rgba, float* out_rgba, size_t pixel_count);

	/// Reconfigure display settings dynamically
	void SetConfig(const HdrDisplayConfig& config);

	[[nodiscard]] const HdrPipelineStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	HdrDisplayConfig      m_config{};
	HdrPipelineStats      m_stats{};
	bool                  m_initialized = false;
};

} // namespace Graphics::HostGpu

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_HDR_PIPELINE_H
