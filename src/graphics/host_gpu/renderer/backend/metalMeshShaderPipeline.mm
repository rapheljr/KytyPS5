// metalMeshShaderPipeline.mm
//
// Metal 3 Mesh Shader Pipeline implementation for macOS Apple Silicon.

#include "graphics/host_gpu/renderer/backend/metalMeshShaderPipeline.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <iostream>

namespace Libs::Graphics {

MetalMeshShaderPipeline::~MetalMeshShaderPipeline() {
	if (m_pipeline_state) {
		id<MTLRenderPipelineState> state = (__bridge id<MTLRenderPipelineState>)m_pipeline_state;
		[state release];
		m_pipeline_state = nullptr;
	}
}

bool MetalMeshShaderPipeline::Initialize(void* mtl_device, const MetalMeshPipelineConfig& config, const std::string& msl_source) {
	m_config = config;
	@autoreleasepool {
		id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;
		if (!device) {
			m_is_valid = false;
			return false;
		}

		if (![device supportsFamily:MTLGPUFamilyMetal3]) {
			m_is_valid = false;
			return false;
		}

		NSError* error = nil;
		NSString* source = [NSString stringWithUTF8String:msl_source.c_str()];
		MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
		options.languageVersion = MTLLanguageVersion3_0;

		id<MTLLibrary> library = [device newLibraryWithSource:source options:options error:&error];
		[options release];
		if (!library || error) {
			m_is_valid = false;
			return false;
		}

		MTLMeshRenderPipelineDescriptor* desc = [[MTLMeshRenderPipelineDescriptor alloc] init];

		if (!config.object_entry_point.empty()) {
			NSString* objName = [NSString stringWithUTF8String:config.object_entry_point.c_str()];
			desc.objectFunction = [library newFunctionWithName:objName];
		}

		if (!config.mesh_entry_point.empty()) {
			NSString* meshName = [NSString stringWithUTF8String:config.mesh_entry_point.c_str()];
			desc.meshFunction = [library newFunctionWithName:meshName];
		}

		if (!config.fragment_entry_point.empty()) {
			NSString* fragName = [NSString stringWithUTF8String:config.fragment_entry_point.c_str()];
			desc.fragmentFunction = [library newFunctionWithName:fragName];
		}

		desc.maxTotalThreadsPerMeshThreadgroup = config.max_threads_per_mesh_threadgroup;
		desc.maxTotalThreadsPerObjectThreadgroup = config.max_threads_per_object_threadgroup;
		desc.alphaToCoverageEnabled = config.alpha_to_coverage_enabled;

		if (config.color_format != 0) {
			desc.colorAttachments[0].pixelFormat = static_cast<MTLPixelFormat>(config.color_format);
		}
		if (config.depth_format != 0) {
			desc.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(config.depth_format);
		}

		id<MTLRenderPipelineState> pipelineState = [device newRenderPipelineStateWithMeshDescriptor:desc options:MTLPipelineOptionNone reflection:nil error:&error];
		[desc release];
		[library release];

		if (!pipelineState || error) {
			m_is_valid = false;
			return false;
		}

		m_pipeline_state = (void*)[pipelineState retain];
		m_is_valid = true;
		return true;
	}
}

void MetalMeshShaderPipeline::DispatchMesh(void* render_encoder, uint32_t threadgroups_x, uint32_t threadgroups_y, uint32_t threadgroups_z) {
	if (!m_is_valid || !m_pipeline_state || !render_encoder) return;

	@autoreleasepool {
		id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)render_encoder;
		id<MTLRenderPipelineState> state = (__bridge id<MTLRenderPipelineState>)m_pipeline_state;

		[encoder setRenderPipelineState:state];
		MTLSize threadgroups = MTLSizeMake(threadgroups_x, threadgroups_y, threadgroups_z);
		MTLSize threadsPerObj = MTLSizeMake(m_config.max_threads_per_object_threadgroup, 1, 1);
		MTLSize threadsPerMesh = MTLSizeMake(m_config.max_threads_per_mesh_threadgroup, 1, 1);

		[encoder drawMeshThreadgroups:threadgroups
		 threadsPerObjectThreadgroup:threadsPerObj
		   threadsPerMeshThreadgroup:threadsPerMesh];
	}
}

} // namespace Libs::Graphics
