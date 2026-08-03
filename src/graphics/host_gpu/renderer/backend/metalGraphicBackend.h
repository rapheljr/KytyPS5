#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"

namespace Libs::Graphics {

class MetalGraphicBackend final : public IGraphicBackend {
public:
	MetalGraphicBackend();
	~MetalGraphicBackend() override;

	[[nodiscard]] GraphicBackendType GetBackendType() const noexcept override { return GraphicBackendType::Metal; }
	[[nodiscard]] const char*        GetBackendName() const noexcept override { return "Metal"; }
	[[nodiscard]] bool               IsSupported() const noexcept override;
	[[nodiscard]] bool               Initialize() override;
	void                             Shutdown() override;
	void                             WaitIdle() override;

private:
	bool m_initialized = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_
