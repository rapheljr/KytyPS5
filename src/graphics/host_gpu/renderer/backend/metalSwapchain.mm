#include "graphics/host_gpu/renderer/backend/metalSwapchain.h"
#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"
#include "common/timer.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include "SDL_syswm.h"
#endif

#include <algorithm>
#include <cstdint>

namespace Libs::Graphics {

#if defined(__APPLE__)

static NSView* GetSDLWindowNSView(SDL_Window* sdl_window) {
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
		Detach();
	}

	NSView* view = GetSDLWindowNSView(sdl_window);
	if (view == nil) {
		return false;
	}
	m_view = (void*)view;

	CAMetalLayer* layer         = [CAMetalLayer layer];
	layer.device                = (__bridge id<MTLDevice>)mtl_device;
	layer.pixelFormat           = ToMTLPixelFormat(format);
	layer.framebufferOnly       = YES;
	layer.maximumDrawableCount  = static_cast<NSUInteger>(max_frames);
	m_triple_buffering          = (max_frames >= 3);

	if (@available(macOS 10.13, *)) {
		layer.displaySyncEnabled = m_display_sync ? YES : NO;
		layer.allowsNextDrawableTimeout = NO;
	}

	m_content_scale             = [view window].backingScaleFactor;
	layer.contentsScale         = static_cast<CGFloat>(m_content_scale);

	[view setWantsLayer:YES];
	[view setLayer:layer];

	m_layer = (void*)CFBridgingRetain(layer);

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

void MetalSwapchain::SetDisplaySyncEnabled(bool enabled) {
	m_display_sync = enabled;
#if defined(__APPLE__)
	if (m_layer != nullptr) {
		CAMetalLayer* layer = (__bridge CAMetalLayer*)m_layer;
		if (@available(macOS 10.13, *)) {
			layer.displaySyncEnabled = enabled ? YES : NO;
		}
	}
#endif
}

void MetalSwapchain::SetSwapInterval(int interval) {
	m_swap_interval = interval;
	SetDisplaySyncEnabled(interval > 0);
}

void MetalSwapchain::SetTripleBuffering(bool enable) {
	m_triple_buffering = enable;
#if defined(__APPLE__)
	if (m_layer != nullptr) {
		CAMetalLayer* layer = (__bridge CAMetalLayer*)m_layer;
		layer.maximumDrawableCount = enable ? 3 : 2;
	}
#endif
}

void MetalSwapchain::HandleWindowMinimize() {
	m_minimized = true;
}

void MetalSwapchain::HandleWindowRestore() {
	m_minimized = false;
}

void MetalSwapchain::HandleDisplayChange() {
#if defined(__APPLE__)
	if (m_layer == nullptr || m_view == nullptr) {
		return;
	}
	NSView* view = (__bridge NSView*)m_view;
	if ([view window] != nil) {
		m_content_scale     = [view window].backingScaleFactor;
		CAMetalLayer* layer = (__bridge CAMetalLayer*)m_layer;
		layer.contentsScale = static_cast<CGFloat>(m_content_scale);
		UpdateDrawableSize(static_cast<uint32_t>(view.bounds.size.width),
		                   static_cast<uint32_t>(view.bounds.size.height));
		layer.drawableSize = CGSizeMake(static_cast<CGFloat>(m_drawable_width),
		                                static_cast<CGFloat>(m_drawable_height));
	}
#endif
}

void MetalSwapchain::SetFullscreen(bool fullscreen) {
	m_fullscreen = fullscreen;
	HandleDisplayChange();
}

void MetalSwapchain::Resize(uint32_t logical_width, uint32_t logical_height) {
#if defined(__APPLE__)
	if (m_layer == nullptr || logical_width == 0 || logical_height == 0) {
		return;
	}
	NSView* view    = (__bridge NSView*)m_view;
	m_content_scale = (view != nil && [view window] != nil)
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
	if (m_minimized) {
		return frame;
	}

#if defined(__APPLE__)
	if (m_layer == nullptr) {
		return frame;
	}

	const uint64_t t0    = Common::Timer::QueryPerformanceCounter();
	CAMetalLayer*  layer = (__bridge CAMetalLayer*)m_layer;
	id<CAMetalDrawable> drawable = [layer nextDrawable];
	const uint64_t t1    = Common::Timer::QueryPerformanceCounter();
	const uint64_t freq  = Common::Timer::QueryPerformanceFrequency();
	const uint64_t dt    = (freq > 0) ? ((t1 - t0) * 1'000'000'000ULL / freq) : 0;

	if (drawable == nil) {
		return frame;
	}

	frame.drawable        = (void*)CFBridgingRetain(drawable);
	frame.texture         = (void*)(__bridge void*)drawable.texture;
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

	const uint64_t t0       = Common::Timer::QueryPerformanceCounter();
	id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)frame.drawable;

	if (command_buffer != nullptr && command_buffer->GetNativeCommandBuffer() != nullptr) {
		id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command_buffer->GetNativeCommandBuffer();
		[cb presentDrawable:drawable];
	} else {
		[drawable present];
	}

	const uint64_t t1   = Common::Timer::QueryPerformanceCounter();
	const uint64_t freq = Common::Timer::QueryPerformanceFrequency();
	const uint64_t dt   = (freq > 0) ? ((t1 - t0) * 1'000'000'000ULL / freq) : 0;

	m_last_present_ns   = dt;
	m_total_present_ns += dt;

	CFBridgingRelease(frame.drawable);
	++m_frames_presented;
#else
	(void)frame; (void)command_buffer;
#endif
}

void MetalSwapchain::PresentDrawableScheduled(const MetalDrawableFrame& frame,
                                               MetalCommandBuffer*        command_buffer,
                                               double                     delay_seconds) {
#if defined(__APPLE__)
	if (frame.drawable == nullptr) {
		return;
	}

	const uint64_t t0       = Common::Timer::QueryPerformanceCounter();
	id<CAMetalDrawable> drawable = (__bridge id<CAMetalDrawable>)frame.drawable;

	if (command_buffer != nullptr && command_buffer->GetNativeCommandBuffer() != nullptr) {
		id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command_buffer->GetNativeCommandBuffer();
		if (@available(macOS 10.15, *)) {
			[cb presentDrawable:drawable afterMinimumDuration:delay_seconds];
		} else {
			[cb presentDrawable:drawable];
		}
	} else {
		[drawable present];
	}

	const uint64_t t1   = Common::Timer::QueryPerformanceCounter();
	const uint64_t freq = Common::Timer::QueryPerformanceFrequency();
	const uint64_t dt   = (freq > 0) ? ((t1 - t0) * 1'000'000'000ULL / freq) : 0;

	m_last_present_ns   = dt;
	m_total_present_ns += dt;

	CFBridgingRelease(frame.drawable);
	++m_frames_presented;
#else
	(void)frame; (void)command_buffer; (void)delay_seconds;
#endif
}

double MetalSwapchain::GetGpuUtilizationPercent() const noexcept {
	if (m_last_acquire_ns == 0) {
		return 100.0;
	}
	// Estimate: target frame budget ~16.66 ms (60 FPS).
	// Idle wait time in AcquireDrawable corresponds to GPU bubble.
	double acquire_ms = static_cast<double>(m_last_acquire_ns) / 1'000'000.0;
	double idle_ratio = acquire_ms / 16.666;
	double util       = (1.0 - std::clamp(idle_ratio, 0.0, 1.0)) * 100.0;
	return util;
}

} // namespace Libs::Graphics
