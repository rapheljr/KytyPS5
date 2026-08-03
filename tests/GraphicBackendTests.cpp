// GraphicBackendTests.cpp
//
// Unit tests verifying Phase A Graphic Backend Interface & Factory runtime selection.

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "GraphicBackendTests: failed: %s\n", text);
		std::abort();
	}
}

void TestDefaultBackend() {
	auto default_type = Libs::Graphics::GraphicBackendFactory::GetDefaultBackendType();
	Check(default_type == Libs::Graphics::GraphicBackendType::Vulkan, "Default backend must be Vulkan");
}

void TestVulkanBackendCreation() {
	auto backend = Libs::Graphics::GraphicBackendFactory::CreateBackend(Libs::Graphics::GraphicBackendType::Vulkan);
	Check(backend != nullptr, "Vulkan backend creation failed");
	Check(backend->GetBackendType() == Libs::Graphics::GraphicBackendType::Vulkan, "Backend type mismatch");
	Check(std::strcmp(backend->GetBackendName(), "Vulkan") == 0, "Backend name mismatch");
	Check(backend->IsSupported(), "Vulkan backend must be supported");

	bool init_ok = backend->Initialize();
	Check(init_ok, "Vulkan backend initialization failed");
	backend->Shutdown();
}

void TestMetalBackendCreation() {
	auto backend = Libs::Graphics::GraphicBackendFactory::CreateBackend(Libs::Graphics::GraphicBackendType::Metal);
	Check(backend != nullptr, "Metal backend creation failed");
	Check(backend->GetBackendType() == Libs::Graphics::GraphicBackendType::Metal, "Backend type mismatch");
	Check(std::strcmp(backend->GetBackendName(), "Metal") == 0, "Backend name mismatch");

#if defined(__APPLE__)
	Check(backend->IsSupported(), "Metal backend must be supported on Apple platforms");
	bool init_ok = backend->Initialize();
	Check(init_ok, "Metal backend initialization failed");
	backend->Shutdown();
#else
	Check(!backend->IsSupported(), "Metal backend should not be supported on non-Apple platforms");
#endif
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Graphic Backend Interface Tests            \n");
	std::printf("====================================================\n\n");

	TestDefaultBackend();
	TestVulkanBackendCreation();
	TestMetalBackendCreation();

	std::printf("GraphicBackendTests: PASSED\n");
	return 0;
}
