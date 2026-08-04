// processManager.cpp
//
// Process Manager & Process Control Block (PCB) Implementation for PS5 Kernel Emulation.

#include "kernel/processManager.h"

namespace Libs::Kernel::Ps5 {

ProcessManager::ProcessManager() {
	// Initialize default init process (PID 1)
	CreateProcess("eboot.bin", 0);
}

uint32_t ProcessManager::CreateProcess(const std::string& name, uint32_t parent_pid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	uint32_t pid = m_next_pid++;

	ProcessControlBlock pcb{};
	pcb.pid        = pid;
	pcb.parent_pid = parent_pid;
	pcb.name       = name;
	pcb.state      = ProcessState::Running;
	pcb.uid        = 0;
	pcb.euid       = 0;
	pcb.gid        = 0;
	pcb.egid       = 0;

	m_processes[pid] = std::move(pcb);
	return pid;
}

bool ProcessManager::ExitProcess(uint32_t pid, int32_t exit_code) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_processes.find(pid);
	if (it == m_processes.end()) {
		return false;
	}

	it->second.state     = ProcessState::Zombie;
	it->second.exit_code = exit_code;
	it->second.handle_table.Clear();
	return true;
}

bool ProcessManager::KillProcess(uint32_t pid, int32_t /*sig*/) {
	return ExitProcess(pid, 128 + 9); // SIGKILL exit
}

int32_t ProcessManager::WaitPID(uint32_t pid, int32_t* status_out, int32_t /*options*/) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_processes.find(pid);
	if (it == m_processes.end()) {
		return -1; // ECHILD
	}

	if (it->second.state == ProcessState::Zombie || it->second.state == ProcessState::Exited) {
		if (status_out) {
			*status_out = it->second.exit_code;
		}
		uint32_t exited_pid = it->second.pid;
		m_processes.erase(it);
		return static_cast<int32_t>(exited_pid);
	}

	return 0; // Still running
}

ProcessControlBlock* ProcessManager::GetProcess(uint32_t pid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_processes.find(pid);
	if (it == m_processes.end()) {
		return nullptr;
	}
	return &it->second;
}

size_t ProcessManager::GetActiveProcessCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	size_t count = 0;
	for (const auto& [pid, pcb] : m_processes) {
		if (pcb.state == ProcessState::Running) {
			count++;
		}
	}
	return count;
}

} // namespace Libs::Kernel::Ps5
