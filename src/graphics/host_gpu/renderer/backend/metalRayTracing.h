// metalRayTracing.h
//
// Hardware-Accelerated Ray Tracing Engine for Apple Silicon Metal.
// Emulates PS5 RDNA2 BVH Acceleration Structures & Ray Query/Intersection Pipeline.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_RAY_TRACING_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_RAY_TRACING_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Graphics::HostGpu {

struct RayTracingGeometryDesc {
	uint64_t vertex_buffer_gpu_addr = 0;
	uint32_t vertex_count           = 0;
	uint32_t vertex_stride          = 12; // 3 * sizeof(float)
	uint64_t index_buffer_gpu_addr  = 0;
	uint32_t index_count            = 0;
	bool     opaque                 = true;
};

struct RayTracingInstanceDesc {
	uint32_t blas_id       = 0;
	float    transform[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0}; // 3x4 affine matrix
	uint32_t mask          = 0xFF;
	uint32_t sbt_offset    = 0;
};

struct RayTracingStats {
	uint32_t total_blas_count        = 0;
	uint32_t total_tlas_count        = 0;
	uint64_t total_as_memory_bytes   = 0;
	uint64_t total_ray_queries_dispatched = 0;
	bool     hardware_raytracing_supported = false;
};

class MetalRayTracingEngine {
public:
	MetalRayTracingEngine();
	~MetalRayTracingEngine();

	KYTY_CLASS_NO_COPY(MetalRayTracingEngine);

	bool Initialize();
	void Shutdown();

	/// Build Bottom-Level Acceleration Structure (BLAS) for a mesh geometry
	uint32_t BuildBlas(const std::vector<RayTracingGeometryDesc>& geometries);

	/// Build Top-Level Acceleration Structure (TLAS) for scene instances
	uint32_t BuildTlas(const std::vector<RayTracingInstanceDesc>& instances);

	/// Dispatch Ray Query Simulation / Test
	bool DispatchRayQuery(uint32_t tlas_id, uint32_t ray_count);

	[[nodiscard]] const RayTracingStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	RayTracingStats       m_stats{};
	bool                  m_initialized = false;
};

} // namespace Graphics::HostGpu

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_RAY_TRACING_H
