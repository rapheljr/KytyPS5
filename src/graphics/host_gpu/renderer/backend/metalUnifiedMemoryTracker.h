// metalUnifiedMemoryTracker.h
//
// Unified Memory Virtual Page Protection & Dirty Range Tracking for Apple Silicon Metal.
// Synchronizes shared CPU-GPU memory buffers with zero unnecessary copies.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_UNIFIED_MEMORY_TRACKER_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_UNIFIED_MEMORY_TRACKER_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Graphics::HostGpu {

struct UnifiedMemoryRegion {
	uint64_t gpu_addr    = 0;
	void*    host_ptr    = nullptr;
	size_t   size_bytes  = 0;
	bool     is_coherent = true;
};

struct UnifiedMemoryStats {
	uint64_t total_tracked_bytes      = 0;
	uint64_t total_dirty_pages_flushed = 0;
	uint32_t sync_barrier_count       = 0;
};

class MetalUnifiedMemoryTracker {
public:
	MetalUnifiedMemoryTracker();
	~MetalUnifiedMemoryTracker();

	KYTY_CLASS_NO_COPY(MetalUnifiedMemoryTracker);

	bool Initialize();
	void Shutdown();

	/// Register a unified CPU-GPU memory buffer
	uint32_t RegisterBuffer(uint64_t gpu_addr, void* host_ptr, size_t size_bytes);

	/// Mark a byte range dirty from CPU write
	void MarkRangeDirty(uint32_t handle, size_t offset, size_t size);

	/// Synchronize dirty ranges with Metal GPU pipeline
	void SynchronizeDirtyRanges(uint32_t handle);

	[[nodiscard]] const UnifiedMemoryStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	UnifiedMemoryStats    m_stats{};
	bool                  m_initialized = false;
};

} // namespace Graphics::HostGpu

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_UNIFIED_MEMORY_TRACKER_H
