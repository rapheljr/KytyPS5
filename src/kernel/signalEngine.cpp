// signalEngine.cpp
//
// POSIX & FreeBSD Signal Engine Implementation for PS5 Kernel Emulation.

#include "kernel/signalEngine.h"

#include <chrono>
#include <thread>

namespace Libs::Kernel::Ps5 {

bool SignalEngine::SetSigaction(uint32_t pid, int32_t sig, const KernelSigaction& act, KernelSigaction* old_act_out) {
	if (sig <= 0 || sig > 64 || sig == KERNEL_SIGKILL) {
		return false; // Cannot catch/ignore SIGKILL
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	auto& actions = m_process_actions[pid];
	if (old_act_out) {
		auto it = actions.find(sig);
		if (it != actions.end()) {
			*old_act_out = it->second;
		} else {
			*old_act_out = KernelSigaction{};
		}
	}
	actions[sig] = act;
	return true;
}

bool SignalEngine::GetSigaction(uint32_t pid, int32_t sig, KernelSigaction* act_out) const {
	if (sig <= 0 || sig > 64) return false;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto p_it = m_process_actions.find(pid);
	if (p_it == m_process_actions.end()) {
		if (act_out) *act_out = KernelSigaction{};
		return true;
	}

	auto s_it = p_it->second.find(sig);
	if (s_it == p_it->second.end()) {
		if (act_out) *act_out = KernelSigaction{};
	} else if (act_out) {
		*act_out = s_it->second;
	}
	return true;
}

bool SignalEngine::SendSignal(uint32_t target_pid, uint32_t target_tid, int32_t sig, const KernelSiginfo* info) {
	if (sig < 0 || sig > 64) return false;
	if (sig == 0) return true; // Null signal test

	std::lock_guard<std::mutex> lock(m_mutex);
	uint32_t tid = (target_tid != 0) ? target_tid : target_pid;
	uint64_t sig_bit = 1ULL << (sig - 1);

	// Check if ignored
	auto p_it = m_process_actions.find(target_pid);
	if (p_it != m_process_actions.end()) {
		auto s_it = p_it->second.find(sig);
		if (s_it != p_it->second.end() && s_it->second.handler == KERNEL_SIG_IGN) {
			return true; // Discarded
		}
	}

	m_thread_pending[tid] |= sig_bit;

	KernelSiginfo sinfo{};
	if (info) {
		sinfo = *info;
	}
	sinfo.si_signo = sig;
	sinfo.si_pid   = target_pid;

	m_thread_queue[tid].push_back(sinfo);
	return true;
}

bool SignalEngine::SetThreadSigmask(uint32_t tid, int32_t how, uint64_t set, uint64_t* old_set_out) {
	std::lock_guard<std::mutex> lock(m_mutex);
	uint64_t current = m_thread_sigmasks[tid];
	if (old_set_out) {
		*old_set_out = current;
	}

	// SIGKILL and SIGSTOP cannot be blocked
	set &= ~((1ULL << (KERNEL_SIGKILL - 1)));

	switch (how) {
		case 1: // SIG_BLOCK
			current |= set;
			break;
		case 2: // SIG_UNBLOCK
			current &= ~set;
			break;
		case 3: // SIG_SETMASK
			current = set;
			break;
		default:
			return false;
	}

	m_thread_sigmasks[tid] = current;
	return true;
}

uint64_t SignalEngine::GetPendingSignals(uint32_t tid) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_thread_pending.find(tid);
	if (it == m_thread_pending.end()) {
		return 0;
	}
	return it->second;
}

bool SignalEngine::WaitSignal(uint32_t tid, uint64_t wait_mask, KernelSiginfo* info_out, uint64_t timeout_us) {
	auto start = std::chrono::steady_clock::now();

	while (true) {
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			uint64_t pending = m_thread_pending[tid] & wait_mask;
			if (pending != 0) {
				// Find lowest bit
				for (int sig = 1; sig <= 64; ++sig) {
					uint64_t bit = 1ULL << (sig - 1);
					if ((pending & bit) != 0) {
						m_thread_pending[tid] &= ~bit;
						auto& queue = m_thread_queue[tid];
						for (auto q_it = queue.begin(); q_it != queue.end(); ++q_it) {
							if (q_it->si_signo == sig) {
								if (info_out) *info_out = *q_it;
								queue.erase(q_it);
								break;
							}
						}
						return true;
					}
				}
			}
		}

		if (timeout_us > 0) {
			auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
			                   std::chrono::steady_clock::now() - start)
			                   .count();
			if (static_cast<uint64_t>(elapsed) >= timeout_us) {
				return false; // Timeout
			}
		}

		std::this_thread::sleep_for(std::chrono::microseconds(100));
	}
}

} // namespace Libs::Kernel::Ps5
