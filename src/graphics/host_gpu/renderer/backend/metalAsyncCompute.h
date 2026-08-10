// metalAsyncCompute.h
//
// Metal Dedicated Asynchronous Compute & Multi-Queue Synchronization Engine.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_ASYNC_COMPUTE_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_ASYNC_COMPUTE_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Libs::Graphics::HostGpu::Metal {

struct AsyncComputeStats {
	uint64_t compute_dispatches_total = 0;
	uint64_t sync_barriers_total      = 0;
	double   last_compute_time_ms     = 0.0;
};

class MetalAsyncComputeEngine {
public:
	MetalAsyncComputeEngine();
	~MetalAsyncComputeEngine();

	KYTY_CLASS_NO_COPY(MetalAsyncComputeEngine);

	bool Initialize();
	void Shutdown();

	/// Dispatch a compute kernel asynchronously on the dedicated compute queue
	bool DispatchCompute(uint32_t threadgroups_x, uint32_t threadgroups_y, uint32_t threadgroups_z,
	                     uint32_t threads_per_group_x = 64, uint32_t threads_per_group_y = 1, uint32_t threads_per_group_z = 1);

	/// Insert cross-queue synchronization barrier (Compute Queue signals event, Render Queue waits)
	bool InsertCrossQueueBarrier();

	/// Wait for all in-flight asynchronous compute commands to complete
	void WaitForIdle();

	[[nodiscard]] const AsyncComputeStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;
	bool                  m_initialized = false;
	AsyncComputeStats     m_stats{};
	mutable std::mutex    m_mutex;
};

} // namespace Libs::Graphics::HostGpu::Metal

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_ASYNC_COMPUTE_H
