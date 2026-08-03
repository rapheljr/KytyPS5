#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKENDFACTORY_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKENDFACTORY_H_

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"

#include <memory>

namespace Libs::Graphics {

class GraphicBackendFactory {
public:
	[[nodiscard]] static std::unique_ptr<IGraphicBackend> CreateBackend(GraphicBackendType type);
	[[nodiscard]] static GraphicBackendType               GetDefaultBackendType() noexcept;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_GRAPHICBACKENDFACTORY_H_
