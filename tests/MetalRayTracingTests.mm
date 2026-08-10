// MetalRayTracingTests.mm
//
// Unit & Integration Tests for Metal Hardware-Accelerated Ray Tracing Engine.

#include "graphics/host_gpu/renderer/backend/metalRayTracing.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Graphics::HostGpu;

static void TestMetalRayTracingLifecycle() {
	std::printf("[TEST] MetalRayTracing_Lifecycle\n");

	MetalRayTracingEngine rt_engine;
	if (!rt_engine.Initialize()) {
		std::fprintf(stderr, "FAIL: MetalRayTracingEngine initialization failed\n");
		std::exit(1);
	}

	if (!rt_engine.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	rt_engine.Shutdown();
	if (rt_engine.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MetalRayTracing_Lifecycle\n");
}

static void TestMetalRayTracingAccelerationStructures() {
	std::printf("[TEST] MetalRayTracing_AccelerationStructures\n");

	MetalRayTracingEngine rt_engine;
	rt_engine.Initialize();

	// 1. Build BLAS
	std::vector<RayTracingGeometryDesc> geoms;
	RayTracingGeometryDesc geom{};
	geom.vertex_count = 36;
	geom.index_count  = 36;
	geoms.push_back(geom);

	uint32_t blas_id = rt_engine.BuildBlas(geoms);
	if (blas_id == 0) {
		std::fprintf(stderr, "FAIL: BuildBlas returned 0\n");
		std::exit(1);
	}

	// 2. Build TLAS
	std::vector<RayTracingInstanceDesc> instances;
	RayTracingInstanceDesc inst{};
	inst.blas_id = blas_id;
	instances.push_back(inst);

	uint32_t tlas_id = rt_engine.BuildTlas(instances);
	if (tlas_id == 0) {
		std::fprintf(stderr, "FAIL: BuildTlas returned 0\n");
		std::exit(1);
	}

	// 3. Dispatch Ray Queries
	bool dispatched = rt_engine.DispatchRayQuery(tlas_id, 1024);
	if (!dispatched) {
		std::fprintf(stderr, "FAIL: DispatchRayQuery failed\n");
		std::exit(1);
	}

	const auto& stats = rt_engine.GetStats();
	if (stats.total_blas_count != 1 || stats.total_tlas_count != 1 || stats.total_ray_queries_dispatched != 1024) {
		std::fprintf(stderr, "FAIL: Stats mismatch (BLAS=%u, TLAS=%u, Rays=%llu)\n",
		             stats.total_blas_count, stats.total_tlas_count, stats.total_ray_queries_dispatched);
		std::exit(1);
	}

	rt_engine.Shutdown();
	std::printf("  [ OK ] MetalRayTracing_AccelerationStructures\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal Hardware Ray Tracing Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalRayTracingLifecycle();
	TestMetalRayTracingAccelerationStructures();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
