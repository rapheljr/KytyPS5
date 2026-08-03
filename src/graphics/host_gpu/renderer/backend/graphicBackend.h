#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKEND_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKEND_H_

#include "common/common.h"

namespace Libs::Graphics {

enum class GraphicBackendType {
	Vulkan,
	Metal,
	None
};

class IGraphicBackend {
public:
	virtual ~IGraphicBackend() = default;

	[[nodiscard]] virtual GraphicBackendType GetBackendType() const noexcept = 0;
	[[nodiscard]] virtual const char*        GetBackendName() const noexcept = 0;
	[[nodiscard]] virtual bool               IsSupported() const noexcept = 0;
	[[nodiscard]] virtual bool               Initialize() = 0;
	virtual void                             Shutdown() = 0;
	virtual void                             WaitIdle() = 0;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKEND_H_
