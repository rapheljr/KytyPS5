// metalMeshShaderPipeline.h
//
// Metal 3 Mesh Shader Pipeline for PS5 Primitive & Amplification Shader Emulation.
// Encapsulates MTLMeshRenderPipelineDescriptor and MTLMeshRenderPipelineState.

#ifndef EMULATOR_GRAPHICS_RENDERER_BACKEND_METAL_MESH_SHADER_PIPELINE_H_
#define EMULATOR_GRAPHICS_RENDERER_BACKEND_METAL_MESH_SHADER_PIPELINE_H_

#include "common/common.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Libs::Graphics {

struct MetalMeshPipelineConfig {
	std::string object_entry_point;
	std::string mesh_entry_point;
	std::string fragment_entry_point;
	uint32_t    max_threads_per_mesh_threadgroup   = 128;
	uint32_t    max_threads_per_object_threadgroup = 128;
	uint32_t    color_format                        = 0; // MTLPixelFormatRGBA8Unorm / etc.
	uint32_t    depth_format                        = 0;
	bool        alpha_to_coverage_enabled           = false;
};

class MetalMeshShaderPipeline {
public:
	MetalMeshShaderPipeline() = default;
	~MetalMeshShaderPipeline();

	KYTY_CLASS_NO_COPY(MetalMeshShaderPipeline);

	/// Compile or initialize Metal 3 Mesh Shader Pipeline State
	bool Initialize(void* mtl_device, const MetalMeshPipelineConfig& config, const std::string& msl_source);

	[[nodiscard]] bool IsValid() const noexcept { return m_is_valid; }
	[[nodiscard]] void* GetPipelineState() const noexcept { return m_pipeline_state; }
	[[nodiscard]] const MetalMeshPipelineConfig& GetConfig() const noexcept { return m_config; }

	/// Dispatch mesh threadgroups via MTLRenderCommandEncoder
	void DispatchMesh(void* render_encoder, uint32_t threadgroups_x, uint32_t threadgroups_y, uint32_t threadgroups_z);

private:
	MetalMeshPipelineConfig m_config{};
	void*                   m_pipeline_state = nullptr; // id<MTLMeshRenderPipelineState>
	bool                    m_is_valid       = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_GRAPHICS_RENDERER_BACKEND_METAL_MESH_SHADER_PIPELINE_H_
