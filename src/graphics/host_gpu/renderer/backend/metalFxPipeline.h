#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace HostGpu::Backend {

enum class MetalFxUpscalingMode {
	Spatial = 0,
	Temporal = 1
};

enum class MetalFxQualityPreset {
	Performance = 0, // 2.0x scale
	Balanced = 1,    // 1.7x scale
	Quality = 2,     // 1.5x scale
	UltraQuality = 3 // 1.3x scale
};

struct MetalFxConfig {
	MetalFxUpscalingMode mode = MetalFxUpscalingMode::Spatial;
	MetalFxQualityPreset preset = MetalFxQualityPreset::Quality;
	uint32_t input_width = 1920;
	uint32_t input_height = 1080;
	uint32_t output_width = 3840;
	uint32_t output_height = 2160;
	float jitter_offset_x = 0.0f;
	float jitter_offset_y = 0.0f;
	float sharpness = 0.5f; // 0.0f - 1.0f
	bool reset_history = false;
};

class MetalFxPipeline {
public:
	MetalFxPipeline();
	~MetalFxPipeline();

	bool Initialize(const MetalFxConfig& config);
	void Shutdown();

	bool IsInitialized() const { return m_initialized; }
	bool IsHardwareSupported() const;

	const MetalFxConfig& GetConfig() const { return m_config; }
	void SetConfig(const MetalFxConfig& config);

	static MetalFxConfig CreatePresetConfig(MetalFxQualityPreset preset, uint32_t output_width, uint32_t output_height, MetalFxUpscalingMode mode = MetalFxUpscalingMode::Spatial);

	float GetScalingFactor() const;
	uint64_t GetProcessedFrameCount() const { return m_frame_count; }

	// Spatial & Temporal upscale encode interfaces (takes raw void* Metal texture pointers)
	bool EncodeSpatialUpscale(void* command_buffer, void* input_texture, void* output_texture);
	bool EncodeTemporalUpscale(void* command_buffer, void* color_texture, void* depth_texture,
	                           void* motion_texture, void* output_texture);

private:
	MetalFxConfig m_config;
	bool m_initialized = false;
	uint64_t m_frame_count = 0;
	void* m_spatial_scaler = nullptr;
	void* m_temporal_scaler = nullptr;
};

} // namespace HostGpu::Backend
