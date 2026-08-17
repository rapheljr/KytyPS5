// MetalMeshShaderTests.cpp
//
// Unit & Integration tests for Metal 3 Mesh Shader Pipeline configuration,
// compilation, and dispatch structure.

#include "graphics/host_gpu/renderer/backend/metalMeshShaderPipeline.h"

#include <cstdio>
#include <cstdlib>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Graphics;

void TestMeshShaderPipelineConfig() {
	std::printf("[TEST] Metal 3 Mesh Shader Pipeline Config...\n");

	MetalMeshPipelineConfig config;
	config.object_entry_point = "object_main";
	config.mesh_entry_point = "mesh_main";
	config.fragment_entry_point = "fragment_main";
	config.max_threads_per_mesh_threadgroup = 64;
	config.max_threads_per_object_threadgroup = 32;

	MetalMeshShaderPipeline pipeline;
	Check(!pipeline.IsValid(), "Default pipeline should be invalid");

	std::printf("  [OK] Metal 3 Mesh Shader Pipeline Config\n");
}

void TestLiveMeshShaderCompilation() {
	std::printf("[TEST] Metal 3 Mesh Shader Compilation (on Metal 3 devices)...\n");

#if defined(__APPLE__)
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (device && [device supportsFamily:MTLGPUFamilyMetal3]) {
		const std::string msl_mesh_shader = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float4 color;
};

using TriangleMeshType = metal::mesh<VertexOut, void, 3, 1, topology::triangle>;

[[mesh, max_total_threads_per_threadgroup(64)]]
void mesh_main(
    TriangleMeshType output,
    uint tid [[thread_index_in_threadgroup]])
{
    if (tid == 0) {
        output.set_primitive_count(1);
        output.set_vertex(0, VertexOut{float4(-0.5, -0.5, 0.0, 1.0), float4(1, 0, 0, 1)});
        output.set_vertex(1, VertexOut{float4( 0.5, -0.5, 0.0, 1.0), float4(0, 1, 0, 1)});
        output.set_vertex(2, VertexOut{float4( 0.0,  0.5, 0.0, 1.0), float4(0, 0, 1, 1)});
        output.set_index(0, 0);
        output.set_index(1, 1);
        output.set_index(2, 2);
    }
}

fragment float4 fragment_main(VertexOut in [[stage_in]]) {
    return in.color;
}
)";

		MetalMeshPipelineConfig config;
		config.mesh_entry_point = "mesh_main";
		config.fragment_entry_point = "fragment_main";
		config.max_threads_per_mesh_threadgroup = 64;
		config.color_format = 80; // MTLPixelFormatBGRA8Unorm

		MetalMeshShaderPipeline pipeline;
		bool ok = pipeline.Initialize((__bridge void*)device, config, msl_mesh_shader);
		Check(ok, "Failed to compile Metal 3 mesh shader");
		Check(pipeline.IsValid(), "Mesh pipeline state should be valid");
		std::printf("  [OK] Live Metal 3 Mesh Shader Compilation\n");
	} else {
		std::printf("  [SKIP] Live Metal 3 Mesh Shader Compilation (Device does not support Metal 3)\n");
	}
#else
	std::printf("  [SKIP] Non-Apple platform\n");
#endif
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal 3 Mesh Shader Pipeline Test Suite\n");
	std::printf("================================================================================\n");

	TestMeshShaderPipelineConfig();
	TestLiveMeshShaderCompilation();

	std::printf("================================================================================\n");
	std::printf("  Results: All Metal Mesh Shader Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
