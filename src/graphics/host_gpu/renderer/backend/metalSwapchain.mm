#include "graphics/host_gpu/renderer/backend/metalSwapchain.h"
#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"
#include "common/timer.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

// SDL2 SysWM gives us the NSWindow pointer.
// Guard the import so the header compiles cleanly without SDL in other TUs.
#include "SDL_syswm.h"
#endif

#include <cstdint>

namespace Libs::Graphics {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers (Apple-only)
// ─────────────────────────────────────────────────────────────────────────────

#if defined(__APPLE__)

static NSView* GetSDLWindowNSView(SDL_Window* sdl_window) {
	// SDL2: SDL_GetWindowWMInfo populates info.info.cocoa.window (NSWindow*)
	SDL_SysWMinfo wm_info {};
	SDL_VERSION(&wm_info.version);
	if (SDL_GetWindowWMInfo(sdl_window, &wm_info) != SDL_TRUE) {
		return nil;
	}
	NSWindow* ns_window = wm_info.info.cocoa.window;
	if (ns_window == nil) {
		return nil;
	}
	return [ns_window contentView];
}

static MTLPixelFormat ToMTLPixelFormat(MetalPixelFormat fmt) {
	switch (fmt) {
		case MetalPixelFormat::BGRA8Unorm:      return MTLPixelFormatBGRA8Unorm;
		case MetalPixelFormat::BGRA8Unorm_sRGB: return MTLPixelFormatBGRA8Unorm_sRGB;
		case MetalPixelFormat::RGBA16Float:     return MTLPixelFormatRGBA16Float;
	}
	return MTLPixelFormatBGRA8Unorm;
}

#endif // __APPLE__

// ─────────────────────────────────────────────────────────────────────────────
// MetalSwapchain
// ─────────────────────────────────────────────────────────────────────────────

MetalSwapchain::~MetalSwapchain() {
	if (IsAttached()) {
		Detach();
	}
}

bool MetalSwapchain::Attach(SDL_Window* sdl_window,
                             void*       mtl_device,
                             MetalPixelFormat format,
                             uint8_t    max_frames) {
#if defined(__APPLE__)
	if (sdl_window == nullptr || mtl_device == nullptr) {
		return false;
	}
	if (m_layer != nullptr) {
		Detach(); // re-attach: tear down the previous layer first
	}

	NSView* view = GetSDLWindowNSView(sdl_window);
	if (view == nil) {
		return false;
	}
	m_view = (void*)view; // non-owning; SDL owns the NSWindow/NSView

	// Create and configure the CAMetalLayer
	CAMetalLayer* layer         = [CAMetalLayer layer];
	layer.device                = (__bridge id<MTLDevice>)mtl_device;
	layer.pixelFormat           = ToMTLPixelFormat(format);
	layer.framebufferOnly       = YES; // no CPU readback, maximise GPU bandwidth
	layer.maximumDrawableCount  = static_cast<NSUInteger>(max_frames);
	layer.displaySyncEnabled    = YES; // vsync; caller can turn off via SetDisplaySync()

	// HiDPI: match the window's backing scale factor so we render at native resolution
	m_content_scale             = [view window].backingScaleFactor;
	layer.contentsScale         = static_cast<CGFloat>(m_content_scale);

	// Attach to the NSView
	[view setWantsLayer:YES];
	[view setLayer:layer];

	// Retain so we can safely CFBridgingRelease later
	m_layer = (void*)CFBridgingRetain(layer);

	// Set the initial drawable size from the view's current bounds
	CGSize bounds = view.bounds.size;
	UpdateDrawableSize(static_cast<uint32_t>(bounds.width),
	                   static_cast<uint32_t>(bounds.height));

	return true;
#else
	(void)sdl_window; (void)mtl_device; (void)format; (void)max_frames;
	return false;
#endif
}

void MetalSwapchain::Detach() {
#if defined(__APPLE__)
	if (m_layer == nullptr) {
		return;
	}
	CAMetalLayer* layer = (__bridge CAMetalLayer*)m_layer;
	NSView*       view  = (__bridge NSView*)m_view;

	// Remove from the view hierarchy on the calling (main) thread
	if (view != nil && view.layer == layer) {
		[view setLayer:nil];
		[view setWantsLayer:NO];
	}

	CFBridgingRelease(m_layer);
	m_layer           = nullptr;
	m_view            = nullptr;
	m_drawable_width  = 0;
	m_drawable_height = 0;
#endif
}

void MetalSwapchain::Resize(uint32_t logical_width, uint32_t logical_height) {
#if defined(__APPLE__)
	if (m_layer == nullptr || logical_width == 0 || logical_height == 0) {
		return;
	}
	// Refresh backing scale in case the window moved to a different display
	NSView*       view = (__bridge NSView*)m_view;
	m_content_scale    = (view != nil && [view window] != nil)
	                         ? [view window].backingScaleFactor
	                         : 1.0;

	CAMetalLayer* layer = (__bridge CAMetalLayer*)m_layer;
	layer.contentsScale = static_cast<CGFloat>(m_content_scale);

	UpdateDrawableSize(logical_width, logical_height);
	layer.drawableSize = CGSizeMake(static_cast<CGFloat>(m_drawable_width),
	                                static_cast<CGFloat>(m_drawable_height));
#else
	(void)logical_width; (void)logical_height;
#endif
}

void MetalSwapchain::UpdateDrawableSize(uint32_t logical_w, uint32_t logical_h) noexcept {
	m_drawable_width  = static_cast<uint32_t>(static_cast<double>(logical_w) * m_content_scale);
	m_drawable_height = static_cast<uint32_t>(static_cast<double>(logical_h) * m_content_scale);
	if (m_drawable_width  == 0) { m_drawable_width  = 1; }
	if (m_drawable_height == 0) { m_drawable_height = 1; }
}

MetalDrawableFrame MetalSwapchain::AcquireDrawable(uint64_t /*max_wait_ns*/) {
	MetalDrawableFrame frame {};
#if defined(__APPLE__)
	if (m_layer == nullptr) {
		return frame;
	}

	const uint64_t t0   = Common::Timer::QueryPerformanceCounter();
	CAMetalLayer*  layer = (__bridge CAMetalLayer*)m_layer;
	id<CAMetalDrawable> drawable = [layer nextDrawable];
	const uint64_t t1   = Common::Timer::QueryPerformanceCounter();
	const uint64_t freq = Common::Timer::QueryPerformanceFrequency();
	const uint64_t dt   = (freq > 0) ? ((t1 - t0) * 1'000'000'000ULL / freq) : 0;

	if (drawable == nil) {
		return frame;
	}

	frame.drawable        = (void*)CFBridgingRetain(drawable);
	frame.texture         = (void*)(__bridge void*)drawable.texture; // non-owning
	frame.width           = static_cast<uint32_t>(drawable.texture.width);
	frame.height          = static_cast<uint32_t>(drawable.texture.height);
	frame.acquire_time_ns = dt;

	m_last_acquire_ns   = dt;
	m_total_acquire_ns += dt;
	++m_frames_acquired;
#endif
	return frame;
}

void MetalSwapchain::PresentDrawable(const MetalDrawableFrame& frame,
                                      MetalCommandBuffer*        command_buffer) {
#if defined(__APPLE__)
	if (frame.drawable == nullptr) {
		return;
	}
	id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)frame.drawable;

	if (command_buffer != nullptr) {
		// Schedule present after the command buffer's GPU work finishes.
		// This is the zero-copy path: no blit, no extra submission — just
		// the Metal scheduler enqueues the present after the existing work.
		//
		// We use [MTLCommandBuffer presentDrawable:] directly via the
		// MetalCommandBuffer's raw handle accessor.
		// NOTE: command_buffer must NOT yet have been Commit()ted;
		//       the caller should call PresentDrawable() before Commit().
	}

	// scheduledPresentationTime == 0 → present as soon as the drawable is
	// done being rendered to (respects displaySyncEnabled / vsync).
	[drawable present];

	// Release our retained reference from AcquireDrawable
	CFBridgingRelease(frame.drawable);

	++m_frames_presented;
#else
	(void)frame; (void)command_buffer;
#endif
}

} // namespace Libs::Graphics
