#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

namespace Libs::Graphics {

VulkanGraphicBackend::VulkanGraphicBackend(): m_context(std::make_unique<GraphicContext>()) {}

VulkanGraphicBackend::~VulkanGraphicBackend() {
	if (m_initialized) {
		Shutdown();
	}
}

bool VulkanGraphicBackend::Initialize() {
	if (m_initialized) {
		return true;
	}
	m_initialized = true;
	return true;
}

void VulkanGraphicBackend::Shutdown() {
	if (!m_initialized) {
		return;
	}
	WaitIdle();
	m_initialized = false;
}

void VulkanGraphicBackend::WaitIdle() {
	if (m_initialized && m_context && m_context->device) {
		(void)m_context->device.waitIdle();
	}
}

} // namespace Libs::Graphics
