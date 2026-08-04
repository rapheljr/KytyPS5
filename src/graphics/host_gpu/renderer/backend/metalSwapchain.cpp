// metalSwapchain_stub.cpp
// Non-Apple stub — all methods return safe fallbacks on platforms where Metal is unavailable.

#include "graphics/host_gpu/renderer/backend/metalSwapchain.h"

#if !defined(__APPLE__)

namespace Libs::Graphics {

MetalSwapchain::~MetalSwapchain() = default;

bool MetalSwapchain::Attach(SDL_Window* /*sdl_window*/,
                            void*       /*mtl_device*/,
                            MetalPixelFormat /*format*/,
                            uint8_t    /*max_frames*/) {
	return false;
}

void MetalSwapchain::Detach() {}

void MetalSwapchain::Resize(uint32_t /*logical_width*/, uint32_t /*logical_height*/) {}

void MetalSwapchain::UpdateDrawableSize(uint32_t /*logical_w*/, uint32_t /*logical_h*/) noexcept {}

MetalDrawableFrame MetalSwapchain::AcquireDrawable(uint64_t /*max_wait_ns*/) {
	return MetalDrawableFrame{};
}

void MetalSwapchain::PresentDrawable(const MetalDrawableFrame& /*frame*/,
                                     MetalCommandBuffer*        /*command_buffer*/) {}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
