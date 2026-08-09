#include "graphics/host_gpu/renderer/backend/metalPipelineCache.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#endif

#include <algorithm>
#include <fstream>
#include <vector>

namespace Libs::Graphics {

#if defined(__APPLE__)

static MTLPixelFormat VulkanFormatToMTLPixelFormat(vk::Format format) {
	switch (format) {
		case vk::Format::eB8G8R8A8Unorm:     return MTLPixelFormatBGRA8Unorm;
		case vk::Format::eB8G8R8A8Srgb:      return MTLPixelFormatBGRA8Unorm_sRGB;
		case vk::Format::eR8G8B8A8Unorm:     return MTLPixelFormatRGBA8Unorm;
		case vk::Format::eR8G8B8A8Srgb:      return MTLPixelFormatRGBA8Unorm_sRGB;
		case vk::Format::eR16G16B16A16Sfloat: return MTLPixelFormatRGBA16Float;
		case vk::Format::eR32G32B32A32Sfloat: return MTLPixelFormatRGBA32Float;
		case vk::Format::eD32Sfloat:         return MTLPixelFormatDepth32Float;
		case vk::Format::eD24UnormS8Uint:    return MTLPixelFormatDepth24Unorm_Stencil8;
		case vk::Format::eD32SfloatS8Uint:   return MTLPixelFormatDepth32Float_Stencil8;
		case vk::Format::eS8Uint:            return MTLPixelFormatStencil8;
		default:                             return MTLPixelFormatBGRA8Unorm;
	}
}

static MTLCompareFunction VulkanCompareOpToMTL(vk::CompareOp op) {
	switch (op) {
		case vk::CompareOp::eNever:          return MTLCompareFunctionNever;
		case vk::CompareOp::eLess:           return MTLCompareFunctionLess;
		case vk::CompareOp::eEqual:          return MTLCompareFunctionEqual;
		case vk::CompareOp::eLessOrEqual:    return MTLCompareFunctionLessEqual;
		case vk::CompareOp::eGreater:        return MTLCompareFunctionGreater;
		case vk::CompareOp::eNotEqual:       return MTLCompareFunctionNotEqual;
		case vk::CompareOp::eGreaterOrEqual: return MTLCompareFunctionGreaterEqual;
		case vk::CompareOp::eAlways:         return MTLCompareFunctionAlways;
	}
	return MTLCompareFunctionAlways;
}

#endif // __APPLE__

MetalPipelineCache::MetalPipelineCache(void* mtl_device,
                                       size_t max_graphics_capacity,
                                       size_t max_compute_capacity)
    : m_device(mtl_device),
      m_max_graphics_capacity(max_graphics_capacity),
      m_max_compute_capacity(max_compute_capacity) {}

MetalPipelineCache::~MetalPipelineCache() {
	Clear();
}

MetalRenderPipelineEntry* MetalPipelineCache::GetOrCreateGraphicsPipeline(const MetalGraphicsPipelineKey& key) {
	Common::LockGuard lock(m_mutex);

	auto it = m_graphics_pipelines.find(key);
	if (it != m_graphics_pipelines.end()) {
		++m_graphics_hits;
		it->second->last_used_timestamp = ++m_timestamp_counter;
		++it->second->use_count;
		return it->second.get();
	}

	++m_graphics_misses;

	// Enforce capacity limit via LRU eviction before compiling new pipeline
	if (m_graphics_pipelines.size() >= m_max_graphics_capacity && m_max_graphics_capacity > 0) {
		EvictLRUGraphics(m_max_graphics_capacity - 1);
	}

	auto entry = std::unique_ptr<MetalRenderPipelineEntry>(CompileGraphicsPipeline(key));
	if (entry == nullptr) {
		return nullptr;
	}

	entry->last_used_timestamp = ++m_timestamp_counter;
	entry->use_count = 1;
	m_total_memory_bytes += entry->estimated_bytes;

	MetalRenderPipelineEntry* ptr = entry.get();
	m_graphics_pipelines.emplace(key, std::move(entry));
	return ptr;
}

MetalComputePipelineEntry* MetalPipelineCache::GetOrCreateComputePipeline(const MetalComputePipelineKey& key) {
	Common::LockGuard lock(m_mutex);

	auto it = m_compute_pipelines.find(key);
	if (it != m_compute_pipelines.end()) {
		++m_compute_hits;
		it->second->last_used_timestamp = ++m_timestamp_counter;
		++it->second->use_count;
		return it->second.get();
	}

	++m_compute_misses;

	if (m_compute_pipelines.size() >= m_max_compute_capacity && m_max_compute_capacity > 0) {
		EvictLRUCompute(m_max_compute_capacity - 1);
	}

	auto entry = std::unique_ptr<MetalComputePipelineEntry>(CompileComputePipeline(key));
	if (entry == nullptr) {
		return nullptr;
	}

	entry->last_used_timestamp = ++m_timestamp_counter;
	entry->use_count = 1;
	m_total_memory_bytes += entry->estimated_bytes;

	MetalComputePipelineEntry* ptr = entry.get();
	m_compute_pipelines.emplace(key, std::move(entry));
	return ptr;
}

MetalRenderPipelineEntry* MetalPipelineCache::CompileGraphicsPipeline(const MetalGraphicsPipelineKey& key) {
	auto* entry = new MetalRenderPipelineEntry();
	entry->estimated_bytes = 4096; // Estimated memory footprint per pipeline state object

#if defined(__APPLE__)
	if (m_device == nullptr) {
		return entry;
	}

	id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;
	NSError* error    = nil;

	// Default fallback MSL strings if empty MSL source supplied in key
	std::string msl_vs = key.msl_vs_code;
	if (msl_vs.empty()) {
		msl_vs = "#include <metal_stdlib>\n"
		         "using namespace metal;\n"
		         "struct VSOut { float4 pos [[position]]; };\n"
		         "vertex VSOut vertex_main(uint vid [[vertex_id]]) {\n"
		         "    VSOut out; out.pos = float4(0,0,0,1);\n"
		         "    return out;\n"
		         "}\n";
	}

	std::string msl_ps = key.msl_ps_code;
	if (msl_ps.empty()) {
		msl_ps = "#include <metal_stdlib>\n"
		         "using namespace metal;\n"
		         "fragment float4 fragment_main() { return float4(1,1,1,1); }\n";
	}

	NSString* vs_source = [NSString stringWithUTF8String:msl_vs.c_str()];
	NSString* ps_source = [NSString stringWithUTF8String:msl_ps.c_str()];

	MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
	options.languageVersion    = MTLLanguageVersion2_4;

	id<MTLLibrary> vs_lib = [dev newLibraryWithSource:vs_source options:options error:&error];
	id<MTLLibrary> ps_lib = [dev newLibraryWithSource:ps_source options:options error:&error];

	if (vs_lib != nil && ps_lib != nil) {
		id<MTLFunction> vertFunc = [vs_lib newFunctionWithName:@"vertex_main"];
		id<MTLFunction> fragFunc = [ps_lib newFunctionWithName:@"fragment_main"];

		if (vertFunc != nil && fragFunc != nil) {
			MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
			desc.vertexFunction   = vertFunc;
			desc.fragmentFunction = fragFunc;

			const uint32_t color_count = std::min(key.rendering.color_count, static_cast<uint32_t>(RENDER_COLOR_ATTACHMENTS_MAX));
			for (uint32_t i = 0; i < color_count; ++i) {
				desc.colorAttachments[i].pixelFormat = VulkanFormatToMTLPixelFormat(key.rendering.color_formats[i]);
			}

			if (key.rendering.depth_format != vk::Format::eUndefined) {
				desc.depthAttachmentPixelFormat = VulkanFormatToMTLPixelFormat(key.rendering.depth_format);
			}
			if (key.rendering.stencil_format != vk::Format::eUndefined) {
				desc.stencilAttachmentPixelFormat = VulkanFormatToMTLPixelFormat(key.rendering.stencil_format);
			}

			id<MTLRenderPipelineState> rps = [dev newRenderPipelineStateWithDescriptor:desc error:&error];
			if (rps != nil) {
				entry->render_pipeline_state = (void*)CFBridgingRetain(rps);
			}
		}
	}

	// Create Depth Stencil State if depth testing is enabled
	if (key.static_params.depth_test_enable || key.static_params.stencil_test_enable) {
		MTLDepthStencilDescriptor* ds_desc = [[MTLDepthStencilDescriptor alloc] init];
		ds_desc.depthCompareFunction = VulkanCompareOpToMTL(key.static_params.depth_compare_op);
		ds_desc.depthWriteEnabled    = key.static_params.depth_write_enable ? YES : NO;

		id<MTLDepthStencilState> dps = [dev newDepthStencilStateWithDescriptor:ds_desc];
		if (dps != nil) {
			entry->depth_stencil_state = (void*)CFBridgingRetain(dps);
		}
	}
#endif

	return entry;
}

MetalComputePipelineEntry* MetalPipelineCache::CompileComputePipeline(const MetalComputePipelineKey& key) {
	auto* entry = new MetalComputePipelineEntry();
	entry->estimated_bytes = 2048;

#if defined(__APPLE__)
	if (m_device == nullptr) {
		return entry;
	}

	id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;
	NSError* error    = nil;

	std::string msl_cs = key.msl_cs_code;
	if (msl_cs.empty()) {
		msl_cs = "#include <metal_stdlib>\n"
		         "using namespace metal;\n"
		         "kernel void compute_main(uint3 tid [[thread_position_in_threadgroup]],\n"
		         "                         uint3 gid [[threadgroup_position_in_grid]],\n"
		         "                         uint  tidx [[thread_index_in_threadgroup]],\n"
		         "                         threadgroup float* lds [[threadgroup(0)]]) {\n"
		         "    threadgroup_barrier(mem_flags::mem_threadgroup);\n"
		         "}\n";
	}

	NSString* cs_source        = [NSString stringWithUTF8String:msl_cs.c_str()];
	MTLCompileOptions* options = [[MTLCompileOptions alloc] init];
	options.languageVersion    = MTLLanguageVersion2_4;

	id<MTLLibrary> cs_lib = [dev newLibraryWithSource:cs_source options:options error:&error];
	if (cs_lib != nil) {
		id<MTLFunction> compFunc = [cs_lib newFunctionWithName:@"compute_main"];
		if (compFunc != nil) {
			id<MTLComputePipelineState> cps = [dev newComputePipelineStateWithFunction:compFunc error:&error];
			if (cps != nil) {
				entry->compute_pipeline_state = (void*)CFBridgingRetain(cps);
			}
		}
	}
#endif

	return entry;
}

void MetalPipelineCache::EvictLRUGraphics(size_t target_capacity) {
	while (m_graphics_pipelines.size() > target_capacity && !m_graphics_pipelines.empty()) {
		auto lru_it = m_graphics_pipelines.begin();
		uint64_t oldest_ts = lru_it->second->last_used_timestamp;

		for (auto it = m_graphics_pipelines.begin(); it != m_graphics_pipelines.end(); ++it) {
			if (it->second->last_used_timestamp < oldest_ts) {
				oldest_ts = it->second->last_used_timestamp;
				lru_it = it;
			}
		}

#if defined(__APPLE__)
		if (lru_it->second->render_pipeline_state != nullptr) {
			CFBridgingRelease(lru_it->second->render_pipeline_state);
			lru_it->second->render_pipeline_state = nullptr;
		}
		if (lru_it->second->depth_stencil_state != nullptr) {
			CFBridgingRelease(lru_it->second->depth_stencil_state);
			lru_it->second->depth_stencil_state = nullptr;
		}
#endif
		if (m_total_memory_bytes >= lru_it->second->estimated_bytes) {
			m_total_memory_bytes -= lru_it->second->estimated_bytes;
		}
		m_graphics_pipelines.erase(lru_it);
	}
}

void MetalPipelineCache::EvictLRUCompute(size_t target_capacity) {
	while (m_compute_pipelines.size() > target_capacity && !m_compute_pipelines.empty()) {
		auto lru_it = m_compute_pipelines.begin();
		uint64_t oldest_ts = lru_it->second->last_used_timestamp;

		for (auto it = m_compute_pipelines.begin(); it != m_compute_pipelines.end(); ++it) {
			if (it->second->last_used_timestamp < oldest_ts) {
				oldest_ts = it->second->last_used_timestamp;
				lru_it = it;
			}
		}

#if defined(__APPLE__)
		if (lru_it->second->compute_pipeline_state != nullptr) {
			CFBridgingRelease(lru_it->second->compute_pipeline_state);
			lru_it->second->compute_pipeline_state = nullptr;
		}
#endif
		if (m_total_memory_bytes >= lru_it->second->estimated_bytes) {
			m_total_memory_bytes -= lru_it->second->estimated_bytes;
		}
		m_compute_pipelines.erase(lru_it);
	}
}

void MetalPipelineCache::Clear() {
	Common::LockGuard lock(m_mutex);

#if defined(__APPLE__)
	for (auto& [key, entry] : m_graphics_pipelines) {
		if (entry->render_pipeline_state != nullptr) {
			CFBridgingRelease(entry->render_pipeline_state);
			entry->render_pipeline_state = nullptr;
		}
		if (entry->depth_stencil_state != nullptr) {
			CFBridgingRelease(entry->depth_stencil_state);
			entry->depth_stencil_state = nullptr;
		}
	}
	for (auto& [key, entry] : m_compute_pipelines) {
		if (entry->compute_pipeline_state != nullptr) {
			CFBridgingRelease(entry->compute_pipeline_state);
			entry->compute_pipeline_state = nullptr;
		}
	}
#endif

	m_graphics_pipelines.clear();
	m_compute_pipelines.clear();
	m_total_memory_bytes = 0;
}

bool MetalPipelineCache::SaveToDisk(const std::filesystem::path& cache_file) const {
	Common::LockGuard lock(m_mutex);
	std::ofstream out(cache_file, std::ios::binary);
	if (!out.is_open()) return false;

	const uint32_t magic = 0x4B595043; // "KYPC"
	const uint32_t version = 1;
	out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
	out.write(reinterpret_cast<const char*>(&version), sizeof(version));

	uint64_t gfx_count = m_graphics_pipelines.size();
	uint64_t comp_count = m_compute_pipelines.size();
	out.write(reinterpret_cast<const char*>(&gfx_count), sizeof(gfx_count));
	out.write(reinterpret_cast<const char*>(&comp_count), sizeof(comp_count));

	return true;
}

bool MetalPipelineCache::LoadFromDisk(const std::filesystem::path& cache_file) {
	Common::LockGuard lock(m_mutex);
	std::ifstream in(cache_file, std::ios::binary);
	if (!in.is_open()) return false;

	uint32_t magic = 0;
	uint32_t version = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	in.read(reinterpret_cast<char*>(&version), sizeof(version));

	if (magic != 0x4B595043 || version != 1) return false;

	uint64_t gfx_count = 0;
	uint64_t comp_count = 0;
	in.read(reinterpret_cast<char*>(&gfx_count), sizeof(gfx_count));
	in.read(reinterpret_cast<char*>(&comp_count), sizeof(comp_count));

	return true;
}

} // namespace Libs::Graphics
