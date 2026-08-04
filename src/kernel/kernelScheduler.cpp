// kernelScheduler.cpp
//
// Priority Scheduler & Multi-Level Feedback Queue Implementation for PS5 Kernel Emulation.

#include "kernel/kernelScheduler.h"

#include <algorithm>

namespace Libs::Kernel::Ps5 {

KernelScheduler::KernelScheduler() = default;

uint32_t KernelScheduler::CreateThread(const std::string& name, uint32_t priority, uint32_t pid, size_t stack_size) {
	std::lock_guard<std::mutex> lock(m_mutex);
	uint32_t tid = m_next_tid++;

	uint32_t norm_prio = std::min(priority, kMaxPriorityLevels);

	ThreadControlBlock tcb{};
	tcb.tid          = tid;
	tcb.pid          = pid;
	tcb.name         = name;
	tcb.priority     = norm_prio;
	tcb.cpu_affinity = kDefaultCpuAffinityMask;
	tcb.state        = SchedulerThreadState::Init;
	tcb.stack_size   = stack_size;

	m_threads[tid] = std::move(tcb);
	return tid;
}

bool KernelScheduler::StartThread(uint32_t tid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}

	it->second.state = SchedulerThreadState::Ready;
	uint32_t prio = std::min(it->second.priority, kMaxPriorityLevels);
	m_ready_queues[prio].push_back(tid);
	return true;
}

bool KernelScheduler::YieldThread(uint32_t tid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}

	if (it->second.state == SchedulerThreadState::Running) {
		it->second.state = SchedulerThreadState::Ready;
		uint32_t prio = std::min(it->second.priority, kMaxPriorityLevels);
		m_ready_queues[prio].push_back(tid);
	}
	return true;
}

bool KernelScheduler::TerminateThread(uint32_t tid, int32_t exit_code) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}

	it->second.state     = SchedulerThreadState::Terminated;
	it->second.exit_code = exit_code;

	// Remove from ready queues
	uint32_t prio = std::min(it->second.priority, kMaxPriorityLevels);
	auto& q = m_ready_queues[prio];
	q.erase(std::remove(q.begin(), q.end(), tid), q.end());

	return true;
}

bool KernelScheduler::SetPriority(uint32_t tid, uint32_t priority) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}

	uint32_t old_prio = std::min(it->second.priority, kMaxPriorityLevels);
	uint32_t new_prio = std::min(priority, kMaxPriorityLevels);
	it->second.priority = new_prio;

	if (it->second.state == SchedulerThreadState::Ready) {
		auto& old_q = m_ready_queues[old_prio];
		old_q.erase(std::remove(old_q.begin(), old_q.end(), tid), old_q.end());
		m_ready_queues[new_prio].push_back(tid);
	}
	return true;
}

bool KernelScheduler::GetPriority(uint32_t tid, uint32_t* priority_out) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}
	if (priority_out) {
		*priority_out = it->second.priority;
	}
	return true;
}

bool KernelScheduler::SetAffinity(uint32_t tid, uint64_t cpu_mask) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}
	it->second.cpu_affinity = cpu_mask;
	return true;
}

bool KernelScheduler::GetAffinity(uint32_t tid, uint64_t* cpu_mask_out) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return false;
	}
	if (cpu_mask_out) {
		*cpu_mask_out = it->second.cpu_affinity;
	}
	return true;
}

uint32_t KernelScheduler::ScheduleNext(uint32_t current_cpu) {
	std::lock_guard<std::mutex> lock(m_mutex);
	uint64_t cpu_bit = 1ULL << current_cpu;

	for (uint32_t prio = 0; prio <= kMaxPriorityLevels; ++prio) {
		auto& q = m_ready_queues[prio];
		for (auto it = q.begin(); it != q.end(); ++it) {
			uint32_t tid = *it;
			auto tcb_it = m_threads.find(tid);
			if (tcb_it != m_threads.end() && (tcb_it->second.cpu_affinity & cpu_bit) != 0) {
				tcb_it->second.state = SchedulerThreadState::Running;
				q.erase(it);
				return tid;
			}
		}
	}
	return 0; // Idle
}

ThreadControlBlock* KernelScheduler::GetTCB(uint32_t tid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_threads.find(tid);
	if (it == m_threads.end()) {
		return nullptr;
	}
	return &it->second;
}

size_t KernelScheduler::GetActiveThreadCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	size_t count = 0;
	for (const auto& [tid, tcb] : m_threads) {
		if (tcb.state == SchedulerThreadState::Running || tcb.state == SchedulerThreadState::Ready) {
			count++;
		}
	}
	return count;
}

size_t KernelScheduler::GetReadyThreadCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	size_t count = 0;
	for (uint32_t prio = 0; prio <= kMaxPriorityLevels; ++prio) {
		count += m_ready_queues[prio].size();
	}
	return count;
}

} // namespace Libs::Kernel::Ps5
