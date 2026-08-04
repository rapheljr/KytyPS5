#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_

// MetalSwapchain — Phase D
//
// Manages the CAMetalLayer attached to an SDL2 window's NSView.
//
// Responsibilities:
//   - Attach / detach CAMetalLayer to the window's NSView on the main thread
//   - Acquire the next MTLDrawable (without rendering to it)
//   - Present the drawable to the display
//   - Track and resize the layer's drawableSize on window resize / DPI change
//   - Measure frame acquisition latency for profiling
//
// NOT implemented here:
//   - Any render-pass or render-encoder usage
//   - Depth/stencil attachment
//   - Pipeline state
//   - Multi-window management (each window owns its own MetalSwapchain)
//
// Thread model:
//   - Attach() / Detach() / Resize() MUST be called from the MAIN THREAD
//     (AppKit CALayer mutations are main-thread-only).
//   - AcquireDrawable() / PresentDrawable() may be called from any thread.

#include <cstdint>

struct SDL_Window; // forward — avoids pulling in SDL.h from this header

namespace Libs::Graphics {

class MetalCommandBuffer; // forward

// ─────────────────────────────────────────────────────────────────────────────
// Pixel format for the CAMetalLayer surface
// ─────────────────────────────────────────────────────────────────────────────
enum class MetalPixelFormat : uint8_t {
	BGRA8Unorm    = 0, // MTLPixelFormatBGRA8Unorm    — sRGB LDR (default)
	BGRA8Unorm_sRGB = 1, // MTLPixelFormatBGRA8Unorm_sRGB — gamma-corrected
	RGBA16Float   = 2, // MTLPixelFormatRGBA16Float   — HDR10 / DisplayP3
};

// ─────────────────────────────────────────────────────────────────────────────
// Frame info returned by AcquireDrawable()
// ─────────────────────────────────────────────────────────────────────────────
struct MetalDrawableFrame {
	void*    drawable        = nullptr; // id<CAMetalDrawable> — nil on failure
	void*    texture         = nullptr; // id<MTLTexture> (the drawable's texture)
	uint32_t width           = 0;
	uint32_t height          = 0;
	uint64_t acquire_time_ns = 0;       // Time spent waiting for drawable (ns)
};

// ─────────────────────────────────────────────────────────────────────────────
// MetalSwapchain
// ─────────────────────────────────────────────────────────────────────────────
class MetalSwapchain final {
public:
	MetalSwapchain() = default;
	~MetalSwapchain();

	// Non-copyable, non-movable (layer ptr held by ObjC runtime)
	MetalSwapchain(const MetalSwapchain&)            = delete;
	MetalSwapchain& operator=(const MetalSwapchain&) = delete;
	MetalSwapchain(MetalSwapchain&&)                 = delete;
	MetalSwapchain& operator=(MetalSwapchain&&)      = delete;

	// ── Lifecycle ────────────────────────────────────────────────────────────

	/// Attach the CAMetalLayer to the NSView of the given SDL window.
	/// Must be called from the main thread.
	/// @param sdl_window   SDL window whose NSView will host the layer.
	/// @param mtl_device   id<MTLDevice> (from MetalGraphicBackend).
	/// @param format       Pixel format for the layer surface.
	/// @param max_frames   Number of drawables the layer can have in-flight (1–3).
	/// @return true on success.
	bool Attach(SDL_Window* sdl_window,
	            void*       mtl_device,
	            MetalPixelFormat format    = MetalPixelFormat::BGRA8Unorm,
	            uint8_t    max_frames      = 3);

	/// Detach and release the CAMetalLayer from the NSView.
	/// Must be called from the main thread.
	void Detach();

	[[nodiscard]] bool IsAttached() const noexcept { return m_layer != nullptr; }

	// ── Size Management ───────────────────────────────────────────────────────

	/// Update drawableSize to match the window's current pixel (backing) size.
	/// Must be called from the main thread after SDL_WINDOWEVENT_RESIZED or
	/// SDL_WINDOWEVENT_SIZE_CHANGED.
	void Resize(uint32_t logical_width, uint32_t logical_height);

	[[nodiscard]] uint32_t GetDrawableWidth()  const noexcept { return m_drawable_width; }
	[[nodiscard]] uint32_t GetDrawableHeight() const noexcept { return m_drawable_height; }

	// ── Frame Lifecycle ───────────────────────────────────────────────────────

	/// Acquire the next drawable from the layer.
	/// Blocks until a drawable is available (up to max_wait_ns nanoseconds; 0 = wait forever).
	/// Returns a MetalDrawableFrame; drawable == nullptr on timeout or error.
	[[nodiscard]] MetalDrawableFrame AcquireDrawable(uint64_t max_wait_ns = 0);

	/// Present the drawable previously acquired by AcquireDrawable().
	/// May be called from any thread.
	/// @param frame           Frame returned by AcquireDrawable().
	/// @param command_buffer  The MetalCommandBuffer that encoded work on frame.texture.
	///                        May be nullptr for a bare present (no GPU work).
	void PresentDrawable(const MetalDrawableFrame& frame, MetalCommandBuffer* command_buffer = nullptr);

	// ── Diagnostics ───────────────────────────────────────────────────────────

	[[nodiscard]] uint64_t GetTotalFramesAcquired()  const noexcept { return m_frames_acquired; }
	[[nodiscard]] uint64_t GetTotalFramesPresented()  const noexcept { return m_frames_presented; }
	[[nodiscard]] uint64_t GetLastAcquireLatencyNs()  const noexcept { return m_last_acquire_ns; }
	[[nodiscard]] double   GetAverageAcquireLatencyNs() const noexcept {
		return (m_frames_acquired > 0)
		    ? static_cast<double>(m_total_acquire_ns) / static_cast<double>(m_frames_acquired)
		    : 0.0;
	}

private:
	void*    m_layer           = nullptr; // CAMetalLayer*
	void*    m_view            = nullptr; // NSView* (non-owning, owned by SDL)
	uint32_t m_drawable_width  = 0;
	uint32_t m_drawable_height = 0;
	double   m_content_scale   = 1.0;    // HiDPI backing scale factor

	// Diagnostics
	uint64_t m_frames_acquired   = 0;
	uint64_t m_frames_presented  = 0;
	uint64_t m_last_acquire_ns   = 0;
	uint64_t m_total_acquire_ns  = 0;

	/// Compute the backing (pixel) size from logical size × content scale.
	void UpdateDrawableSize(uint32_t logical_w, uint32_t logical_h) noexcept;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSWAPCHAIN_H_
