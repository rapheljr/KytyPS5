#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDQUEUE_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDQUEUE_H_

// MetalCommandQueue — Phase C
//
// A thin, C++ wrapper around a single id<MTLCommandQueue>.
//
// Responsibilities:
//   - Create MetalCommandBuffer instances on demand.
//   - Track in-flight command buffers.
//   - Block until all submitted work is complete (WaitAllCompleted).
//
// Thread-safety: MetalCommandQueue itself is not thread-safe.
//               Callers must serialize access externally.

#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Libs::Graphics {

class MetalCommandQueue final {
public:
	/// Wraps an existing id<MTLCommandQueue> raw pointer (must not be nil).
	/// The queue does NOT take ownership — the lifetime is managed by
	/// MetalGraphicBackend which owns both the device and the queue.
	explicit MetalCommandQueue(void* mtl_command_queue) noexcept;
	~MetalCommandQueue();

	// Non-copyable, movable
	MetalCommandQueue(const MetalCommandQueue&)            = delete;
	MetalCommandQueue& operator=(const MetalCommandQueue&) = delete;
	MetalCommandQueue(MetalCommandQueue&&) noexcept;
	MetalCommandQueue& operator=(MetalCommandQueue&&) noexcept;

	// ── State ─────────────────────────────────────────────────────────────────

	[[nodiscard]] bool IsValid() const noexcept { return m_queue != nullptr; }

	// ── Command Buffer Creation ───────────────────────────────────────────────

	/// Allocates a new MetalCommandBuffer in Recording state.
	/// Returns nullptr if the queue is invalid.
	[[nodiscard]] std::unique_ptr<MetalCommandBuffer> CreateCommandBuffer();

	// ── Synchronization ───────────────────────────────────────────────────────

	/// Blocks until every MetalCommandBuffer that was created by this queue
	/// and has been Commit()ted has also reached the Completed state.
	void WaitAllCompleted();

	// ── Diagnostics ───────────────────────────────────────────────────────────

	/// Total number of command buffers created since the queue was constructed.
	[[nodiscard]] uint64_t GetTotalCommandBuffersCreated() const noexcept { return m_total_created; }

	/// Total number of completed command buffers.
	[[nodiscard]] uint64_t GetTotalCommandBuffersCompleted() const noexcept { return m_total_completed; }

	/// Retrieve the raw id<MTLCommandQueue> pointer (for testing / introspection).
	[[nodiscard]] void* GetMTLCommandQueue() const noexcept { return m_queue; }

private:
	void*    m_queue           = nullptr; // id<MTLCommandQueue>, non-owning
	uint64_t m_total_created   = 0;
	uint64_t m_total_completed = 0;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALCOMMANDQUEUE_H_
