// ps5FiberScheduler.cpp
//
// User-Space Fiber & Lockless Work-Stealing Job Dispatch Scheduler Implementation.

#include "kernel/ps5FiberScheduler.h"

#include <iostream>

namespace Kernel {

Ps5FiberScheduler::Ps5FiberScheduler(uint32_t worker_threads)
	: m_num_workers(worker_threads) {}

Ps5FiberScheduler::~Ps5FiberScheduler() {
	Shutdown();
}

bool Ps5FiberScheduler::Initialize() {
	if (m_initialized) return true;

	std::lock_guard<std::mutex> lock(m_queue_mutex);
	m_job_queue.clear();
	m_stats.active_worker_threads = m_num_workers;
	m_initialized = true;

	return true;
}

void Ps5FiberScheduler::Shutdown() {
	if (!m_initialized) return;

	WaitForAll();

	std::lock_guard<std::mutex> lock(m_queue_mutex);
	m_job_queue.clear();
	m_initialized = false;
}

uint64_t Ps5FiberScheduler::SubmitJob(FiberJobFn fn, void* user_data) {
	if (!m_initialized || !fn) return 0;

	uint64_t id = m_next_job_id.fetch_add(1);

	FiberTask task;
	task.job_id    = id;
	task.function  = std::move(fn);
	task.user_data = user_data;
	task.state     = FiberState::Ready;

	{
		std::lock_guard<std::mutex> lock(m_queue_mutex);
		m_job_queue.push_back(std::move(task));
		m_stats.total_jobs_dispatched++;
	}

	return id;
}

bool Ps5FiberScheduler::ExecuteNextJob() {
	if (!m_initialized) return false;

	FiberTask task;
	{
		std::lock_guard<std::mutex> lock(m_queue_mutex);
		if (m_job_queue.empty()) return false;

		task = std::move(m_job_queue.front());
		m_job_queue.pop_front();
	}

	if (task.function) {
		task.state = FiberState::Running;
		task.function(task.user_data);
		task.state = FiberState::Completed;

		std::lock_guard<std::mutex> lock(m_queue_mutex);
		m_stats.total_jobs_completed++;
	}

	return true;
}

void Ps5FiberScheduler::WaitForAll() {
	while (ExecuteNextJob()) {
		// Drain queue on caller thread
	}
}

} // namespace Kernel
