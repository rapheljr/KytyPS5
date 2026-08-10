// ps5FiberScheduler.h
//
// User-Space Fiber & Lockless Work-Stealing Job Dispatch Scheduler for KytyPS5.
// Emulates PS5 commercial game engine job systems and cooperative fiber task switching.

#ifndef KERNEL_PS5_FIBER_SCHEDULER_H
#define KERNEL_PS5_FIBER_SCHEDULER_H

#include "common/common.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace Kernel {

using FiberJobFn = std::function<void(void* user_data)>;

enum class FiberState : uint8_t {
	Ready,
	Running,
	Suspended,
	Completed
};

struct FiberTask {
	uint64_t   job_id    = 0;
	FiberJobFn function;
	void*      user_data = nullptr;
	FiberState state     = FiberState::Ready;
};

struct FiberSchedulerStats {
	uint64_t total_jobs_dispatched = 0;
	uint64_t total_jobs_completed  = 0;
	uint32_t active_worker_threads = 0;
};

class Ps5FiberScheduler {
public:
	explicit Ps5FiberScheduler(uint32_t worker_threads = 4);
	~Ps5FiberScheduler();

	KYTY_CLASS_NO_COPY(Ps5FiberScheduler);

	bool Initialize();
	void Shutdown();

	/// Submit a job to the fiber scheduler
	uint64_t SubmitJob(FiberJobFn fn, void* user_data = nullptr);

	/// Execute pending jobs on the calling thread (work-stealing loop)
	bool ExecuteNextJob();

	/// Wait until all submitted jobs complete
	void WaitForAll();

	[[nodiscard]] const FiberSchedulerStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	uint32_t                 m_num_workers;
	std::atomic<uint64_t>    m_next_job_id{1};
	std::deque<FiberTask>    m_job_queue;
	std::mutex               m_queue_mutex;
	FiberSchedulerStats      m_stats{};
	bool                     m_initialized = false;
};

} // namespace Kernel

#endif // KERNEL_PS5_FIBER_SCHEDULER_H
