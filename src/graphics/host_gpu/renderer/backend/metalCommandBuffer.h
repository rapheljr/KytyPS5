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

#include <cstddef>
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

	/// Dispatches direct compute threadgroups.
	void DispatchThreadgroups(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
	                          uint32_t threads_per_group_x, uint32_t threads_per_group_y, uint32_t threads_per_group_z);

	/// Dispatches indirect compute threadgroups from GPU buffer.
	void DispatchIndirect(void* indirect_buffer, size_t indirect_offset,
	                      uint32_t threads_per_group_x, uint32_t threads_per_group_y, uint32_t threads_per_group_z);

	// ── Render Encoder ────────────────────────────────────────────────────────
	/// Opens an MTLRenderCommandEncoder with a given MTLRenderPassDescriptor.
	[[nodiscard]] void* OpenRenderEncoder(void* mtl_render_pass_descriptor);

	/// Closes the currently open render encoder (calls endEncoding).
	void CloseRenderEncoder();

	/// Sets viewport on the active render encoder.
	void SetViewport(float x, float y, float width, float height, float znear = 0.0f, float zfar = 1.0f);

	/// Sets scissor rectangle on the active render encoder.
	void SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

	/// Sets the active render pipeline state.
	void SetRenderPipelineState(void* mtl_pipeline_state);

	/// Sets a vertex buffer on the active render encoder.
	void SetVertexBuffer(void* mtl_buffer, size_t offset, uint32_t index);

	/// Issues a non-indexed draw call.
	void DrawPrimitives(uint32_t primitive_type, uint32_t vertex_start, uint32_t vertex_count, uint32_t instance_count = 1);

	/// Issues an indexed draw call.
	void DrawIndexedPrimitives(uint32_t primitive_type, uint32_t index_count, uint32_t index_type,
	                           void* index_buffer, size_t index_buffer_offset, uint32_t instance_count = 1);

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
	[[nodiscard]] void* GetNativeRenderEncoder() const noexcept { return m_render_encoder; }

	// ── Diagnostics ──────────────────────────────────────────────────────────

	/// Time in nanoseconds between commit and GPU completion (0 if not complete).
	[[nodiscard]] uint64_t GetGpuExecutionTimeNs() const noexcept;

private:
	void*                  m_command_buffer   = nullptr; // id<MTLCommandBuffer>
	void*                  m_compute_encoder  = nullptr; // id<MTLComputeCommandEncoder>; valid only while open
	void*                  m_render_encoder   = nullptr; // id<MTLRenderCommandEncoder>; valid only while open
	MetalCommandBufferState m_state            = MetalCommandBufferState::NotAllocated;
	uint64_t               m_gpu_time_ns      = 0;
	bool                   m_waited           = false;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDBUFFER_H_
