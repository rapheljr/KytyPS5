// ps5Kernel.cpp
//
// Syscall Dispatcher, Thread Manager & Process Manager for Phase N PS5 Kernel Emulation.

#include "kernel/ps5Kernel.h"

namespace Libs::Kernel::Ps5 {

uint32_t ThreadManager::CreateThread(const std::string& name, uint32_t priority, size_t stack_size) {
	uint32_t tid = m_next_thread_id++;
	ThreadInfo info{};
	info.thread_id  = tid;
	info.name       = name;
	info.priority   = priority;
	info.state      = ThreadState::Init;
	info.stack_size = stack_size;

	m_threads[tid] = info;
	return tid;
}

bool ThreadManager::StartThread(uint32_t thread_id) {
	auto it = m_threads.find(thread_id);
	if (it == m_threads.end()) return false;
	it->second.state = ThreadState::Running;
	return true;
}

bool ThreadManager::TerminateThread(uint32_t thread_id) {
	auto it = m_threads.find(thread_id);
	if (it == m_threads.end()) return false;
	it->second.state = ThreadState::Terminated;
	return true;
}

ThreadInfo* ThreadManager::GetThreadInfo(uint32_t thread_id) {
	auto it = m_threads.find(thread_id);
	if (it == m_threads.end()) return nullptr;
	return &it->second;
}

size_t ThreadManager::GetActiveThreadCount() const noexcept {
	size_t count = 0;
	for (const auto& [tid, info] : m_threads) {
		if (info.state == ThreadState::Running || info.state == ThreadState::Ready) {
			count++;
		}
	}
	return count;
}

// ─── Syscall Dispatcher ──────────────────────────────────────────────────────

SyscallDispatcher::SyscallDispatcher() {
	// Register default dummy handlers for core kernel syscalls
	RegisterSyscall(1, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_exit
	RegisterSyscall(6, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_close
}

void SyscallDispatcher::RegisterSyscall(uint32_t num, SyscallFunc handler) {
	if (handler) {
		m_syscall_map[num] = handler;
	}
}

int64_t SyscallDispatcher::Dispatch(uint32_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5) {
	auto it = m_syscall_map.find(num);
	if (it == m_syscall_map.end()) {
		return -1; // ENOSYS
	}
	return it->second(a0, a1, a2, a3, a4, a5);
}

} // namespace Libs::Kernel::Ps5
