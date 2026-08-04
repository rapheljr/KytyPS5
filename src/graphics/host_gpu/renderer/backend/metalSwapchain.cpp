#include "graphics/host_gpu/renderer/backend/metalSwapchain.h"

namespace Libs::Graphics {

MetalSwapchain::~MetalSwapchain() = default;

bool MetalSwapchain::Attach(SDL_Window* /*sdl_window*/,
                             void*       /*mtl_device*/,
                             MetalPixelFormat /*format*/,
                             uint8_t    /*max_frames*/) {
	return false;
}

void MetalSwapchain::Detach() {}

void MetalSwapchain::SetDisplaySyncEnabled(bool enabled) { m_display_sync = enabled; }
void MetalSwapchain::SetSwapInterval(int interval) { m_swap_interval = interval; SetDisplaySyncEnabled(interval > 0); }
void MetalSwapchain::SetTripleBuffering(bool enable) { m_triple_buffering = enable; }

void MetalSwapchain::HandleWindowMinimize() { m_minimized = true; }
void MetalSwapchain::HandleWindowRestore() { m_minimized = false; }
void MetalSwapchain::HandleDisplayChange() {}
void MetalSwapchain::SetFullscreen(bool fullscreen) { m_fullscreen = fullscreen; }

void MetalSwapchain::Resize(uint32_t logical_width, uint32_t logical_height) {
	UpdateDrawableSize(logical_width, logical_height);
}

void MetalSwapchain::UpdateDrawableSize(uint32_t logical_w, uint32_t logical_h) noexcept {
	m_drawable_width  = logical_w;
	m_drawable_height = logical_h;
}

MetalDrawableFrame MetalSwapchain::AcquireDrawable(uint64_t /*max_wait_ns*/) {
	return MetalDrawableFrame {};
}

void MetalSwapchain::PresentDrawable(const MetalDrawableFrame& /*frame*/,
                                      MetalCommandBuffer*        /*command_buffer*/) {}

void MetalSwapchain::PresentDrawableScheduled(const MetalDrawableFrame& /*frame*/,
                                               MetalCommandBuffer*        /*command_buffer*/,
                                               double                     /*delay_seconds*/) {}

double MetalSwapchain::GetGpuUtilizationPercent() const noexcept {
	return 0.0;
}

} // namespace Libs::Graphics
