// processManager.h
//
// Process Manager & Process Control Block (PCB) for PS5 Kernel Emulation.

#ifndef KERNEL_PROCESS_MANAGER_H
#define KERNEL_PROCESS_MANAGER_H

#include "common/common.h"
#include "kernel/kernelObject.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

enum class ProcessState : uint8_t {
	Init = 0,
	Running,
	Zombie,
	Exited
};

struct SignalAction {
	uint64_t handler  = 0; // 0 = SIG_DFL, 1 = SIG_IGN
	uint64_t mask     = 0;
	int32_t  flags    = 0;
};

struct ProcessControlBlock {
	uint32_t           pid        = 1;
	uint32_t           parent_pid = 0;
	std::string        name;
	ProcessState       state      = ProcessState::Init;
	uint32_t           uid        = 0;
	uint32_t           euid       = 0;
	uint32_t           gid        = 0;
	uint32_t           egid       = 0;
	int32_t            exit_code  = 0;
	HandleTable        handle_table;
	std::vector<uint32_t> thread_ids;
	std::unordered_map<int32_t, SignalAction> signal_actions;
};

class ProcessManager {
public:
	ProcessManager();
	~ProcessManager() = default;

	KYTY_CLASS_NO_COPY(ProcessManager);

	uint32_t CreateProcess(const std::string& name, uint32_t parent_pid = 0);
	bool ExitProcess(uint32_t pid, int32_t exit_code);
	bool KillProcess(uint32_t pid, int32_t sig);
	int32_t WaitPID(uint32_t pid, int32_t* status_out, int32_t options);

	[[nodiscard]] ProcessControlBlock* GetProcess(uint32_t pid);
	[[nodiscard]] uint32_t GetCurrentPid() const noexcept { return m_current_pid; }
	void SetCurrentPid(uint32_t pid) noexcept { m_current_pid = pid; }

	[[nodiscard]] size_t GetActiveProcessCount() const;

private:
	mutable std::mutex                              m_mutex;
	uint32_t                                        m_next_pid = 1;
	uint32_t                                        m_current_pid = 1;
	std::unordered_map<uint32_t, ProcessControlBlock> m_processes;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PROCESS_MANAGER_H
