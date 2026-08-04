// ps5Kernel.h
//
// Syscall Dispatcher & Master PS5 Kernel Service Subsystem for Phase N PS5 Kernel Emulation.

#ifndef KERNEL_PS5_KERNEL_H
#define KERNEL_PS5_KERNEL_H

#include "common/common.h"
#include "kernel/ipcSharedMemory.h"
#include "kernel/kernelObject.h"
#include "kernel/kernelScheduler.h"
#include "kernel/kernelTimer.h"
#include "kernel/processManager.h"
#include "kernel/ps5Umtx.h"
#include "kernel/signalEngine.h"

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
	uint32_t    priority   = 256;
	ThreadState state      = ThreadState::Init;
	uint64_t    stack_ptr  = 0;
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

	[[nodiscard]] ProcessManager& GetProcessManager() noexcept { return m_process_manager; }
	[[nodiscard]] KernelScheduler& GetScheduler() noexcept { return m_scheduler; }
	[[nodiscard]] KernelTimerManager& GetTimerManager() noexcept { return m_timer_manager; }
	[[nodiscard]] SignalEngine& GetSignalEngine() noexcept { return m_signal_engine; }
	[[nodiscard]] UmtxManager& GetUmtxManager() noexcept { return m_umtx_manager; }
	[[nodiscard]] PipeManager& GetPipeManager() noexcept { return m_pipe_manager; }
	[[nodiscard]] SharedMemoryManager& GetSharedMemoryManager() noexcept { return m_shm_manager; }
	[[nodiscard]] MessageQueueManager& GetMessageQueueManager() noexcept { return m_mq_manager; }

private:
	void RegisterAllFreeBSDSyscalls();

	std::unordered_map<uint32_t, SyscallFunc> m_syscall_map;

	ProcessManager      m_process_manager;
	KernelScheduler     m_scheduler;
	KernelTimerManager  m_timer_manager;
	SignalEngine        m_signal_engine;
	UmtxManager         m_umtx_manager;
	PipeManager         m_pipe_manager;
	SharedMemoryManager m_shm_manager;
	MessageQueueManager m_mq_manager;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_KERNEL_H
