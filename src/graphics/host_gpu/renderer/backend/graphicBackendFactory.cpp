#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"

#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

#include <cstdlib>
#include <cstring>

#include "common/emulatorConfig.h"

namespace Libs::Graphics {

std::unique_ptr<IGraphicBackend> GraphicBackendFactory::CreateBackend(GraphicBackendType type) {
	switch (type) {
		case GraphicBackendType::Vulkan: return std::make_unique<VulkanGraphicBackend>();
		case GraphicBackendType::Metal: return std::make_unique<MetalGraphicBackend>();
		default: return nullptr;
	}
}

GraphicBackendType GraphicBackendFactory::GetDefaultBackendType() noexcept {
	// 1. Check Config options if explicitly set
	auto cfg_choice = Config::GetGraphicBackend();
	if (cfg_choice == Config::GraphicBackendChoice::Metal) {
		return GraphicBackendType::Metal;
	}
	if (cfg_choice == Config::GraphicBackendChoice::Vulkan) {
		return GraphicBackendType::Vulkan;
	}

	// 2. Check environment variable override
	const char* env_backend = std::getenv("KYTY_GRAPHICS_BACKEND");
	if (env_backend != nullptr) {
		if (std::strcmp(env_backend, "metal") == 0 || std::strcmp(env_backend, "Metal") == 0) {
			return GraphicBackendType::Metal;
		}
		if (std::strcmp(env_backend, "vulkan") == 0 || std::strcmp(env_backend, "Vulkan") == 0) {
			return GraphicBackendType::Vulkan;
		}
	}

	// 3. Platform default (Apple -> Metal, others -> Vulkan)
#if defined(__APPLE__)
	return GraphicBackendType::Metal;
#else
	return GraphicBackendType::Vulkan;
#endif
}

} // namespace Libs::Graphics
