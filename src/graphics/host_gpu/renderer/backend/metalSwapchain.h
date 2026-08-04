#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_

// MetalSwapchain — Final Metal Presentation & Frame Pacing
//
// Manages the CAMetalLayer attached to an SDL2 window's NSView.
//
// Responsibilities:
//   - Attach / detach CAMetalLayer to the window's NSView on the main thread
//   - Acquire next MTLDrawable with frame acquisition latency tracking
//   - Present drawable directly or scheduled via MTLCommandBuffer
//   - Hardware VSync control (displaySyncEnabled / swapInterval)
//   - Double / Triple buffering toggle (maximumDrawableCount 2 vs 3)
//   - Window state management: minimize/restore, fullscreen toggle, multi-monitor display changes
//   - Latency, CPU overhead, and GPU utilization diagnostic profiling

#include <cstdint>

struct SDL_Window; // forward — avoids pulling in SDL.h from header

namespace Libs::Graphics {

class MetalCommandBuffer; // forward

enum class MetalPixelFormat : uint8_t {
	BGRA8Unorm      = 0, // MTLPixelFormatBGRA8Unorm    — sRGB LDR (default)
	BGRA8Unorm_sRGB = 1, // MTLPixelFormatBGRA8Unorm_sRGB — gamma-corrected
	RGBA16Float     = 2, // MTLPixelFormatRGBA16Float   — HDR10 / DisplayP3
};

struct MetalDrawableFrame {
	void*    drawable        = nullptr; // id<CAMetalDrawable> — nil on failure
	void*    texture         = nullptr; // id<MTLTexture> (the drawable's texture)
	uint32_t width           = 0;
	uint32_t height          = 0;
	uint64_t acquire_time_ns = 0;       // Time spent waiting for drawable (ns)
};

class MetalSwapchain final {
public:
	MetalSwapchain() = default;
	~MetalSwapchain();

	// Non-copyable, non-movable
	MetalSwapchain(const MetalSwapchain&)            = delete;
	MetalSwapchain& operator=(const MetalSwapchain&) = delete;
	MetalSwapchain(MetalSwapchain&&)                 = delete;
	MetalSwapchain& operator=(MetalSwapchain&&)      = delete;

	// ── Lifecycle ────────────────────────────────────────────────────────────

	bool Attach(SDL_Window* sdl_window,
	            void*       mtl_device,
	            MetalPixelFormat format    = MetalPixelFormat::BGRA8Unorm,
	            uint8_t    max_frames      = 3);

	void Detach();

	[[nodiscard]] bool IsAttached() const noexcept { return m_layer != nullptr; }

	// ── VSync & Pacing Configuration ──────────────────────────────────────────

	void SetDisplaySyncEnabled(bool enabled);
	[[nodiscard]] bool IsDisplaySyncEnabled() const noexcept { return m_display_sync; }

	void SetSwapInterval(int interval);
	[[nodiscard]] int GetSwapInterval() const noexcept { return m_swap_interval; }

	void SetTripleBuffering(bool enable);
	[[nodiscard]] bool IsTripleBufferingEnabled() const noexcept { return m_triple_buffering; }

	// ── Window State & Multi-Monitor Transitions ──────────────────────────────

	void HandleWindowMinimize();
	void HandleWindowRestore();
	[[nodiscard]] bool IsMinimized() const noexcept { return m_minimized; }

	void HandleDisplayChange();

	void SetFullscreen(bool fullscreen);
	[[nodiscard]] bool IsFullscreen() const noexcept { return m_fullscreen; }

	// ── Size Management ───────────────────────────────────────────────────────

	void Resize(uint32_t logical_width, uint32_t logical_height);

	[[nodiscard]] uint32_t GetDrawableWidth()  const noexcept { return m_drawable_width; }
	[[nodiscard]] uint32_t GetDrawableHeight() const noexcept { return m_drawable_height; }

	// ── Frame Lifecycle & Presentation ────────────────────────────────────────

	[[nodiscard]] MetalDrawableFrame AcquireDrawable(uint64_t max_wait_ns = 0);

	void PresentDrawable(const MetalDrawableFrame& frame, MetalCommandBuffer* command_buffer = nullptr);

	void PresentDrawableScheduled(const MetalDrawableFrame& frame,
	                              MetalCommandBuffer*        command_buffer,
	                              double                     delay_seconds);

	// ── Diagnostics & Profiling ───────────────────────────────────────────────

	[[nodiscard]] uint64_t GetTotalFramesAcquired()  const noexcept { return m_frames_acquired; }
	[[nodiscard]] uint64_t GetTotalFramesPresented() const noexcept { return m_frames_presented; }
	[[nodiscard]] uint64_t GetLastAcquireLatencyNs() const noexcept { return m_last_acquire_ns; }
	[[nodiscard]] uint64_t GetLastPresentLatencyNs() const noexcept { return m_last_present_ns; }
	[[nodiscard]] double   GetAverageAcquireLatencyNs() const noexcept {
		return (m_frames_acquired > 0)
		    ? static_cast<double>(m_total_acquire_ns) / static_cast<double>(m_frames_acquired)
		    : 0.0;
	}
	[[nodiscard]] double   GetAveragePresentLatencyNs() const noexcept {
		return (m_frames_presented > 0)
		    ? static_cast<double>(m_total_present_ns) / static_cast<double>(m_frames_presented)
		    : 0.0;
	}
	[[nodiscard]] uint64_t GetEstimatedCpuOverheadNs() const noexcept { return m_last_acquire_ns + m_last_present_ns; }
	[[nodiscard]] double   GetGpuUtilizationPercent() const noexcept;

private:
	void*    m_layer           = nullptr; // CAMetalLayer*
	void*    m_view            = nullptr; // NSView* (non-owning)
	uint32_t m_drawable_width  = 0;
	uint32_t m_drawable_height = 0;
	double   m_content_scale   = 1.0;

	bool     m_display_sync     = true;
	int      m_swap_interval    = 1;
	bool     m_triple_buffering = true;
	bool     m_minimized        = false;
	bool     m_fullscreen       = false;

	// Diagnostics
	uint64_t m_frames_acquired   = 0;
	uint64_t m_frames_presented  = 0;
	uint64_t m_last_acquire_ns   = 0;
	uint64_t m_total_acquire_ns  = 0;
	uint64_t m_last_present_ns   = 0;
	uint64_t m_total_present_ns  = 0;

	void UpdateDrawableSize(uint32_t logical_w, uint32_t logical_h) noexcept;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_
