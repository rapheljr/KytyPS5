#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_VULKANGRAPHICBACKEND_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_VULKANGRAPHICBACKEND_H_

#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/backend/graphicBackend.h"

#include <memory>

namespace Libs::Graphics {

class VulkanGraphicBackend final : public IGraphicBackend {
public:
	VulkanGraphicBackend();
	~VulkanGraphicBackend() override;

	[[nodiscard]] GraphicBackendType GetBackendType() const noexcept override { return GraphicBackendType::Vulkan; }
	[[nodiscard]] const char*        GetBackendName() const noexcept override { return "Vulkan"; }
	[[nodiscard]] bool               IsSupported() const noexcept override { return true; }
	[[nodiscard]] bool               Initialize() override;
	void                             Shutdown() override;
	void                             WaitIdle() override;

	[[nodiscard]] GraphicContext* GetContext() noexcept { return m_context.get(); }

private:
	std::unique_ptr<GraphicContext> m_context;
	bool                            m_initialized = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_VULKANGRAPHICBACKEND_H_
