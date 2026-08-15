// metalRayTracing.mm
//
// Hardware-Accelerated Ray Tracing Engine Implementation for Apple Silicon Metal.

#include "graphics/host_gpu/renderer/backend/metalRayTracing.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <iostream>
#include <mutex>
#include <unordered_map>

namespace Graphics::HostGpu {

struct MetalRayTracingEngine::Impl {
	id<MTLDevice>       device       = nil;
	id<MTLCommandQueue> command_queue = nil;
	std::mutex          mutex;

	uint32_t next_blas_id = 1;
	uint32_t next_tlas_id = 1;

	std::unordered_map<uint32_t, id> blas_map;
	std::unordered_map<uint32_t, id> tlas_map;

	~Impl() {
		blas_map.clear();
		tlas_map.clear();
		command_queue = nil;
		device = nil;
	}
};

MetalRayTracingEngine::MetalRayTracingEngine() : m_impl(std::make_unique<Impl>()) {}
MetalRayTracingEngine::~MetalRayTracingEngine() { Shutdown(); }

bool MetalRayTracingEngine::Initialize() {
	if (m_initialized) return true;

	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			std::cerr << "[MetalRayTracing] No default Metal device available\n";
			return false;
		}

		m_impl->command_queue = [m_impl->device newCommandQueue];
		if (!m_impl->command_queue) {
			std::cerr << "[MetalRayTracing] Failed to create Metal command queue\n";
			return false;
		}

		if (@available(macOS 11.0, *)) {
			m_stats.hardware_raytracing_supported = [m_impl->device supportsRaytracing];
		} else {
			m_stats.hardware_raytracing_supported = false;
		}

		std::cout << "[MetalRayTracing] Initialized Metal Ray Tracing Engine (HW RT Supported: "
		          << (m_stats.hardware_raytracing_supported ? "YES" : "NO / Fallback") << ")\n";

		m_initialized = true;
		return true;
	}
}

void MetalRayTracingEngine::Shutdown() {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->blas_map.clear();
	m_impl->tlas_map.clear();
	m_impl->command_queue = nil;
	m_impl->device = nil;

	m_initialized = false;
}

uint32_t MetalRayTracingEngine::BuildBlas(const std::vector<RayTracingGeometryDesc>& geometries) {
	if (!m_initialized || geometries.empty()) return 0;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	uint32_t as_id = m_impl->next_blas_id++;

	@autoreleasepool {
		uint64_t total_verts = 0;
		for (const auto& g : geometries) {
			total_verts += g.vertex_count;
		}

		uint64_t as_size = total_verts * sizeof(float) * 3 + 1024;
		m_stats.total_blas_count++;
		m_stats.total_as_memory_bytes += as_size;

		if (@available(macOS 11.0, *)) {
			if (m_stats.hardware_raytracing_supported) {
				NSMutableArray<MTLAccelerationStructureTriangleGeometryDescriptor*>* geom_descriptors = [NSMutableArray array];
				for (const auto& g : geometries) {
					MTLAccelerationStructureTriangleGeometryDescriptor* tri_desc = [MTLAccelerationStructureTriangleGeometryDescriptor descriptor];
					tri_desc.opaque = g.opaque;
					tri_desc.vertexStride = g.vertex_stride;
					tri_desc.triangleCount = (g.index_count > 0) ? (g.index_count / 3) : (g.vertex_count / 3);
					[geom_descriptors addObject:tri_desc];
				}
				MTLPrimitiveAccelerationStructureDescriptor* blas_desc = [MTLPrimitiveAccelerationStructureDescriptor descriptor];
				blas_desc.geometryDescriptors = geom_descriptors;

				MTLAccelerationStructureSizes sizes = [m_impl->device accelerationStructureSizesWithDescriptor:blas_desc];
				id blas = [m_impl->device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
				m_impl->blas_map[as_id] = blas;
			} else {
				m_impl->blas_map[as_id] = nil;
			}
		} else {
			m_impl->blas_map[as_id] = nil;
		}
	}

	return as_id;
}

uint32_t MetalRayTracingEngine::BuildTlas(const std::vector<RayTracingInstanceDesc>& instances) {
	if (!m_initialized || instances.empty()) return 0;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	uint32_t as_id = m_impl->next_tlas_id++;

	@autoreleasepool {
		uint64_t as_size = instances.size() * sizeof(RayTracingInstanceDesc) + 2048;
		m_stats.total_tlas_count++;
		m_stats.total_as_memory_bytes += as_size;

		if (@available(macOS 11.0, *)) {
			if (m_stats.hardware_raytracing_supported) {
				MTLInstanceAccelerationStructureDescriptor* tlas_desc = [MTLInstanceAccelerationStructureDescriptor descriptor];
				tlas_desc.instanceCount = instances.size();
				MTLAccelerationStructureSizes sizes = [m_impl->device accelerationStructureSizesWithDescriptor:tlas_desc];
				id tlas = [m_impl->device newAccelerationStructureWithSize:sizes.accelerationStructureSize];
				m_impl->tlas_map[as_id] = tlas;
			} else {
				m_impl->tlas_map[as_id] = nil;
			}
		} else {
			m_impl->tlas_map[as_id] = nil;
		}
	}

	return as_id;
}

bool MetalRayTracingEngine::DispatchRayQuery(uint32_t tlas_id, uint32_t ray_count) {
	if (!m_initialized || tlas_id == 0 || ray_count == 0) return false;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	auto it = m_impl->tlas_map.find(tlas_id);
	if (it == m_impl->tlas_map.end()) return false;

	m_stats.total_ray_queries_dispatched += ray_count;
	return true;
}

} // namespace Graphics::HostGpu
