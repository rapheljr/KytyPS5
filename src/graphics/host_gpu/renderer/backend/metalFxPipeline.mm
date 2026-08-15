// metalFxPipeline.mm
//
// MetalFX Super Resolution Spatial & Temporal Upscaling Pipeline Implementation.

#include "graphics/host_gpu/renderer/backend/metalFxPipeline.h"
#include "common/logging/log.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>

namespace HostGpu::Backend {

MetalFxPipeline::MetalFxPipeline() = default;

MetalFxPipeline::~MetalFxPipeline() {
	Shutdown();
}

bool MetalFxPipeline::IsHardwareSupported() const {
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device) {
		return false;
	}

	bool supported = false;
	if (m_config.mode == MetalFxUpscalingMode::Spatial) {
		if (@available(macOS 13.0, *)) {
			supported = [MTLFXSpatialScalerDescriptor supportsDevice:device];
		}
	} else {
		if (@available(macOS 13.0, *)) {
			supported = [MTLFXTemporalScalerDescriptor supportsDevice:device];
		}
	}

	[device release];
	return supported;
}

bool MetalFxPipeline::Initialize(const MetalFxConfig& config) {
	if (m_initialized) {
		Shutdown();
	}

	m_config = config;
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device) {
		LOGF("MetalFxPipeline: Failed to create default Metal device\n");
		return false;
	}

	@autoreleasepool {
		if (m_config.mode == MetalFxUpscalingMode::Spatial) {
			if (@available(macOS 13.0, *)) {
				if ([MTLFXSpatialScalerDescriptor supportsDevice:device]) {
					MTLFXSpatialScalerDescriptor* desc = [[MTLFXSpatialScalerDescriptor alloc] init];
					desc.inputWidth = m_config.input_width;
					desc.inputHeight = m_config.input_height;
					desc.outputWidth = m_config.output_width;
					desc.outputHeight = m_config.output_height;
					desc.colorTextureFormat = MTLPixelFormatRGBA8Unorm;
					desc.outputTextureFormat = MTLPixelFormatRGBA8Unorm;
					desc.colorProcessingMode = MTLFXSpatialScalerColorProcessingModePerceptual;

					id<MTLFXSpatialScaler> scaler = [desc newSpatialScalerWithDevice:device];
					m_spatial_scaler = (void*)[scaler retain];
					[desc release];
				}
			}
		} else {
			if (@available(macOS 13.0, *)) {
				if ([MTLFXTemporalScalerDescriptor supportsDevice:device]) {
					MTLFXTemporalScalerDescriptor* desc = [[MTLFXTemporalScalerDescriptor alloc] init];
					desc.inputWidth = m_config.input_width;
					desc.inputHeight = m_config.input_height;
					desc.outputWidth = m_config.output_width;
					desc.outputHeight = m_config.output_height;
					desc.colorTextureFormat = MTLPixelFormatRGBA8Unorm;
					desc.depthTextureFormat = MTLPixelFormatDepth32Float;
					desc.motionTextureFormat = MTLPixelFormatRG16Float;
					desc.outputTextureFormat = MTLPixelFormatRGBA8Unorm;

					id<MTLFXTemporalScaler> scaler = [desc newTemporalScalerWithDevice:device];
					m_temporal_scaler = (void*)[scaler retain];
					[desc release];
				}
			}
		}
	}

	[device release];
	m_frame_count = 0;
	m_initialized = true;
	return true;
}

void MetalFxPipeline::Shutdown() {
	if (!m_initialized) {
		return;
	}

	@autoreleasepool {
		if (m_spatial_scaler) {
			id<MTLFXSpatialScaler> scaler = (id<MTLFXSpatialScaler>)m_spatial_scaler;
			[scaler release];
			m_spatial_scaler = nullptr;
		}
		if (m_temporal_scaler) {
			id<MTLFXTemporalScaler> scaler = (id<MTLFXTemporalScaler>)m_temporal_scaler;
			[scaler release];
			m_temporal_scaler = nullptr;
		}
	}

	m_initialized = false;
	m_frame_count = 0;
}

void MetalFxPipeline::SetConfig(const MetalFxConfig& config) {
	m_config = config;
	if (m_initialized) {
		Initialize(m_config);
	}
}

MetalFxConfig MetalFxPipeline::CreatePresetConfig(MetalFxQualityPreset preset, uint32_t output_width, uint32_t output_height, MetalFxUpscalingMode mode) {
	MetalFxConfig cfg;
	cfg.mode = mode;
	cfg.preset = preset;
	cfg.output_width = output_width;
	cfg.output_height = output_height;

	float scale_factor = 1.5f;
	switch (preset) {
		case MetalFxQualityPreset::Performance:   scale_factor = 2.0f; break;
		case MetalFxQualityPreset::Balanced:      scale_factor = 1.7f; break;
		case MetalFxQualityPreset::Quality:       scale_factor = 1.5f; break;
		case MetalFxQualityPreset::UltraQuality:  scale_factor = 1.3f; break;
	}

	cfg.input_width = static_cast<uint32_t>(std::round(output_width / scale_factor));
	cfg.input_height = static_cast<uint32_t>(std::round(output_height / scale_factor));
	// Ensure even dimensions
	cfg.input_width &= ~1u;
	cfg.input_height &= ~1u;

	return cfg;
}

float MetalFxPipeline::GetScalingFactor() const {
	if (m_config.input_width == 0 || m_config.input_height == 0) {
		return 1.0f;
	}
	float scale_x = static_cast<float>(m_config.output_width) / static_cast<float>(m_config.input_width);
	float scale_y = static_cast<float>(m_config.output_height) / static_cast<float>(m_config.input_height);
	return (scale_x + scale_y) * 0.5f;
}

bool MetalFxPipeline::EncodeSpatialUpscale(void* command_buffer, void* input_texture, void* output_texture) {
	if (!m_initialized || !command_buffer || !input_texture || !output_texture) {
		return false;
	}

	@autoreleasepool {
		if (m_spatial_scaler) {
			id<MTLFXSpatialScaler> scaler = (id<MTLFXSpatialScaler>)m_spatial_scaler;
			id<MTLCommandBuffer> cmd_buf = (id<MTLCommandBuffer>)command_buffer;
			scaler.colorTexture = (id<MTLTexture>)input_texture;
			scaler.outputTexture = (id<MTLTexture>)output_texture;
			[scaler encodeToCommandBuffer:cmd_buf];
		}
	}

	m_frame_count++;
	return true;
}

bool MetalFxPipeline::EncodeTemporalUpscale(void* command_buffer, void* color_texture, void* depth_texture,
                                           void* motion_texture, void* output_texture) {
	if (!m_initialized || !command_buffer || !color_texture || !output_texture) {
		return false;
	}

	@autoreleasepool {
		if (m_temporal_scaler) {
			id<MTLFXTemporalScaler> scaler = (id<MTLFXTemporalScaler>)m_temporal_scaler;
			id<MTLCommandBuffer> cmd_buf = (id<MTLCommandBuffer>)command_buffer;
			scaler.colorTexture = (id<MTLTexture>)color_texture;
			scaler.depthTexture = (id<MTLTexture>)depth_texture;
			scaler.motionTexture = (id<MTLTexture>)motion_texture;
			scaler.outputTexture = (id<MTLTexture>)output_texture;
			scaler.jitterOffsetX = m_config.jitter_offset_x;
			scaler.jitterOffsetY = m_config.jitter_offset_y;
			scaler.reset = m_config.reset_history;
			[scaler encodeToCommandBuffer:cmd_buf];
		}
	}

	m_frame_count++;
	return true;
}

} // namespace HostGpu::Backend
