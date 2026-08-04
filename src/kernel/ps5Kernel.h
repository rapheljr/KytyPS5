// ps5Kernel.h
//
// Syscall Dispatcher, Thread Manager & Process Manager for Phase N PS5 Kernel Emulation.

#ifndef KERNEL_PS5_KERNEL_H
#define KERNEL_PS5_KERNEL_H

#include "common/common.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Libs::Kernel::Ps5 {

using SyscallFunc = int64_t (*)(uint64_t arg0, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5);

enum class ThreadState : uint8_t {
	Init = 0,
	Ready,
	Running,
	Waiting,
	Terminated
};

struct ThreadInfo {
	uint32_t    thread_id = 0;
	std::string name;
	uint32_t    priority  = 256;
	ThreadState state     = ThreadState::Init;
	uint64_t    stack_ptr = 0;
	size_t      stack_size = 1024 * 1024;
};

class ThreadManager {
public:
	ThreadManager() = default;
	~ThreadManager() = default;

	KYTY_CLASS_NO_COPY(ThreadManager);

	uint32_t CreateThread(const std::string& name, uint32_t priority, size_t stack_size);
	bool StartThread(uint32_t thread_id);
	bool TerminateThread(uint32_t thread_id);
	[[nodiscard]] ThreadInfo* GetThreadInfo(uint32_t thread_id);
	[[nodiscard]] size_t GetActiveThreadCount() const noexcept;

private:
	uint32_t m_next_thread_id = 1;
	std::unordered_map<uint32_t, ThreadInfo> m_threads;
};

class SyscallDispatcher {
public:
	SyscallDispatcher();
	~SyscallDispatcher() = default;

	KYTY_CLASS_NO_COPY(SyscallDispatcher);

	void RegisterSyscall(uint32_t num, SyscallFunc handler);
	int64_t Dispatch(uint32_t num, uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5);

	[[nodiscard]] size_t GetRegisteredSyscallCount() const noexcept { return m_syscall_map.size(); }

private:
	std::unordered_map<uint32_t, SyscallFunc> m_syscall_map;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_KERNEL_H
