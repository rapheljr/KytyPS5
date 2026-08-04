// kernelScheduler.h
//
// Priority Scheduler & Multi-Level Feedback Queue for PS5 Kernel Emulation.

#ifndef KERNEL_SCHEDULER_H
#define KERNEL_SCHEDULER_H

#include "common/common.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

enum class SchedulerThreadState : uint8_t {
	Init = 0,
	Ready,
	Running,
	Waiting,
	Suspended,
	Terminated
};

constexpr uint32_t kMaxPriorityLevels = 256;
constexpr uint64_t kDefaultCpuAffinityMask = 0xFFFFFFFF; // 32 cores default

struct ThreadControlBlock {
	uint32_t             tid           = 0;
	uint32_t             pid           = 1;
	std::string          name;
	uint32_t             priority      = 256; // 0 (highest) to 255 (lowest), 256 = default
	uint64_t             cpu_affinity  = kDefaultCpuAffinityMask;
	SchedulerThreadState state         = SchedulerThreadState::Init;
	uint64_t             stack_ptr     = 0;
	size_t               stack_size    = 1024 * 1024;
	uint64_t             tls_ptr       = 0;
	int32_t              exit_code     = 0;
};

class KernelScheduler {
public:
	KernelScheduler();
	~KernelScheduler() = default;

	KYTY_CLASS_NO_COPY(KernelScheduler);

	uint32_t CreateThread(const std::string& name, uint32_t priority = 256, uint32_t pid = 1, size_t stack_size = 1024 * 1024);
	bool StartThread(uint32_t tid);
	bool YieldThread(uint32_t tid);
	bool TerminateThread(uint32_t tid, int32_t exit_code = 0);

	bool SetPriority(uint32_t tid, uint32_t priority);
	bool GetPriority(uint32_t tid, uint32_t* priority_out) const;

	bool SetAffinity(uint32_t tid, uint64_t cpu_mask);
	bool GetAffinity(uint32_t tid, uint64_t* cpu_mask_out) const;

	uint32_t ScheduleNext(uint32_t current_cpu = 0);

	[[nodiscard]] ThreadControlBlock* GetTCB(uint32_t tid);
	[[nodiscard]] size_t GetActiveThreadCount() const;
	[[nodiscard]] size_t GetReadyThreadCount() const;

private:
	mutable std::mutex                                     m_mutex;
	uint32_t                                               m_next_tid = 1;
	std::unordered_map<uint32_t, ThreadControlBlock>        m_threads;
	std::deque<uint32_t>                                   m_ready_queues[kMaxPriorityLevels + 1];
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_SCHEDULER_H
