#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDBUFFER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDBUFFER_H_

// MetalCommandBuffer — Phase C
//
// Wraps a single id<MTLCommandBuffer>.
// Provides:
//   - OpenComputeEncoder() / CloseComputeEncoder()   (compute work; Phase C)
//   - Commit()                                        (enqueue to GPU)
//   - WaitUntilCompleted()                            (CPU-side synchronization)
//
// Rendering encoders (MTLRenderCommandEncoder) will be added in a later phase.
// No swapchain, no pipelines, no rendering in this phase.

#include <cstdint>

namespace Libs::Graphics {

// ─────────────────────────────────────────────────────────────────────────────
// Status of a command buffer across its lifecycle
// ─────────────────────────────────────────────────────────────────────────────
enum class MetalCommandBufferState : uint8_t {
	NotAllocated = 0, // Not yet created
	Recording    = 1, // Allocated, encoders may be opened/closed
	Committed    = 2, // Submitted to GPU queue; no further encoding allowed
	Completed    = 3, // GPU finished; safe to reclaim
	Error        = 4, // Commit or execution error
};

// ─────────────────────────────────────────────────────────────────────────────
// MetalCommandBuffer
// ─────────────────────────────────────────────────────────────────────────────
class MetalCommandBuffer final {
public:
	explicit MetalCommandBuffer(void* mtl_command_queue);
	~MetalCommandBuffer();

	// Non-copyable, non-movable
	MetalCommandBuffer(const MetalCommandBuffer&)            = delete;
	MetalCommandBuffer& operator=(const MetalCommandBuffer&) = delete;
	MetalCommandBuffer(MetalCommandBuffer&&)                 = delete;
	MetalCommandBuffer& operator=(MetalCommandBuffer&&)      = delete;

	// ── Lifecycle ────────────────────────────────────────────────────────────

	/// Returns true if the buffer was allocated successfully.
	[[nodiscard]] bool IsValid() const noexcept;

	/// Current lifecycle state.
	[[nodiscard]] MetalCommandBufferState GetState() const noexcept;

	// ── Compute Encoder ───────────────────────────────────────────────────────

	/// Opens an MTLComputeCommandEncoder.  Must be closed before Commit().
	/// Returns the raw id<MTLComputeCommandEncoder> as void*.
	[[nodiscard]] void* OpenComputeEncoder();

	/// Closes the currently open compute encoder (calls endEncoding).
	void CloseComputeEncoder();

	// ── Submission & Synchronization ─────────────────────────────────────────

	/// Commits (enqueues) this buffer to the GPU.  Must not be in Error state.
	/// After Commit the buffer state becomes Committed.
	void Commit();

	/// Blocks the calling thread until the GPU finishes executing this buffer.
	/// Automatically called by the destructor if the buffer was Committed but
	/// not yet waited on.
	void WaitUntilCompleted();

	/// Raw handle accessors
	[[nodiscard]] void* GetNativeCommandBuffer() const noexcept { return m_command_buffer; }
	[[nodiscard]] void* GetNativeComputeEncoder() const noexcept { return m_compute_encoder; }

	// ── Diagnostics ──────────────────────────────────────────────────────────

	/// Time in nanoseconds between commit and GPU completion (0 if not complete).
	[[nodiscard]] uint64_t GetGpuExecutionTimeNs() const noexcept;

private:
	void*                  m_command_buffer   = nullptr; // id<MTLCommandBuffer>
	void*                  m_compute_encoder  = nullptr; // id<MTLComputeCommandEncoder>; valid only while open
	MetalCommandBufferState m_state            = MetalCommandBufferState::NotAllocated;
	uint64_t               m_gpu_time_ns      = 0;
	bool                   m_waited           = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDBUFFER_H_
