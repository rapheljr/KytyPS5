#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"

#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

namespace Libs::Graphics {

std::unique_ptr<IGraphicBackend> GraphicBackendFactory::CreateBackend(GraphicBackendType type) {
	switch (type) {
		case GraphicBackendType::Vulkan: return std::make_unique<VulkanGraphicBackend>();
		case GraphicBackendType::Metal: return std::make_unique<MetalGraphicBackend>();
		default: return nullptr;
	}
}

GraphicBackendType GraphicBackendFactory::GetDefaultBackendType() noexcept {
	return GraphicBackendType::Vulkan;
}

} // namespace Libs::Graphics
