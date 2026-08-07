// MetalRuntimeIntegrationTests.cpp
//
// Unit & Integration Tests for GraphicBackendFactory and Native Metal Integration.

#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"

#include <cstdio>
#include <cstdlib>

using namespace Libs::Graphics;

static void TestDefaultGraphicBackendSelection() {
	std::printf("[TEST] DefaultGraphicBackendSelection\n");

	// Unset environment variable first
	unsetenv("KYTY_GRAPHICS_BACKEND");

	GraphicBackendType default_type = GraphicBackendFactory::GetDefaultBackendType();
#if defined(__APPLE__)
	if (default_type != GraphicBackendType::Metal) {
		std::fprintf(stderr, "FAIL: Expected default backend to be Metal on macOS\n");
		std::exit(1);
	}
#else
	if (default_type != GraphicBackendType::Vulkan) {
		std::fprintf(stderr, "FAIL: Expected default backend to be Vulkan on non-macOS\n");
		std::exit(1);
	}
#endif

	std::printf("  [ OK ] DefaultGraphicBackendSelection\n");
}

static void TestEnvironmentBackendOverride() {
	std::printf("[TEST] EnvironmentBackendOverride\n");

	setenv("KYTY_GRAPHICS_BACKEND", "vulkan", 1);
	if (GraphicBackendFactory::GetDefaultBackendType() != GraphicBackendType::Vulkan) {
		std::fprintf(stderr, "FAIL: KYTY_GRAPHICS_BACKEND=vulkan override failed\n");
		std::exit(1);
	}

	setenv("KYTY_GRAPHICS_BACKEND", "metal", 1);
	if (GraphicBackendFactory::GetDefaultBackendType() != GraphicBackendType::Metal) {
		std::fprintf(stderr, "FAIL: KYTY_GRAPHICS_BACKEND=metal override failed\n");
		std::exit(1);
	}

	unsetenv("KYTY_GRAPHICS_BACKEND");
	std::printf("  [ OK ] EnvironmentBackendOverride\n");
}

static void TestGraphicBackendCreation() {
	std::printf("[TEST] GraphicBackendCreation\n");

	auto metal_backend = GraphicBackendFactory::CreateBackend(GraphicBackendType::Metal);
	if (!metal_backend) {
		std::fprintf(stderr, "FAIL: Metal graphic backend creation returned null\n");
		std::exit(1);
	}

	if (metal_backend->GetBackendType() != GraphicBackendType::Metal) {
		std::fprintf(stderr, "FAIL: Metal graphic backend reported wrong type\n");
		std::exit(1);
	}

	auto vulkan_backend = GraphicBackendFactory::CreateBackend(GraphicBackendType::Vulkan);
	if (!vulkan_backend) {
		std::fprintf(stderr, "FAIL: Vulkan graphic backend creation returned null\n");
		std::exit(1);
	}

	if (vulkan_backend->GetBackendType() != GraphicBackendType::Vulkan) {
		std::fprintf(stderr, "FAIL: Vulkan graphic backend reported wrong type\n");
		std::exit(1);
	}

	std::printf("  [ OK ] GraphicBackendCreation (Metal: %s, Vulkan: %s)\n",
	            metal_backend->GetBackendName(), vulkan_backend->GetBackendName());
}

int main() {
	std::printf("=========================================\n");
	std::printf(" Running Metal Runtime Integration Tests \n");
	std::printf("=========================================\n");

	TestDefaultGraphicBackendSelection();
	TestEnvironmentBackendOverride();
	TestGraphicBackendCreation();

	std::printf("All Metal Runtime Integration Tests PASSED 100%%!\n");
	return 0;
}
