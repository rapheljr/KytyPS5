// signalEngine.h
//
// POSIX & FreeBSD Signal Engine for PS5 Kernel Emulation.

#ifndef KERNEL_SIGNAL_ENGINE_H
#define KERNEL_SIGNAL_ENGINE_H

#include "common/common.h"

#include <cstdint>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

constexpr int32_t KERNEL_SIGHUP  = 1;
constexpr int32_t KERNEL_SIGINT  = 2;
constexpr int32_t KERNEL_SIGQUIT = 3;
constexpr int32_t KERNEL_SIGILL  = 4;
constexpr int32_t KERNEL_SIGABRT = 6;
constexpr int32_t KERNEL_SIGFPE  = 8;
constexpr int32_t KERNEL_SIGKILL = 9;
constexpr int32_t KERNEL_SIGBUS  = 10;
constexpr int32_t KERNEL_SIGSEGV = 11;
constexpr int32_t KERNEL_SIGPIPE = 13;
constexpr int32_t KERNEL_SIGALRM = 14;
constexpr int32_t KERNEL_SIGTERM = 15;
constexpr int32_t KERNEL_SIGCHLD = 20;
constexpr int32_t KERNEL_SIGUSR1 = 30;
constexpr int32_t KERNEL_SIGUSR2 = 31;

constexpr uint64_t KERNEL_SIG_DFL = 0;
constexpr uint64_t KERNEL_SIG_IGN = 1;

struct KernelSigaction {
	uint64_t handler  = KERNEL_SIG_DFL;
	uint64_t sa_mask  = 0;
	int32_t  sa_flags = 0;
};

struct KernelSiginfo {
	int32_t  si_signo = 0;
	int32_t  si_errno = 0;
	int32_t  si_code  = 0;
	uint32_t si_pid   = 0;
	uint32_t si_uid   = 0;
	uint64_t si_addr  = 0;
	uint64_t value    = 0;
};

class SignalEngine {
public:
	SignalEngine() = default;
	~SignalEngine() = default;

	KYTY_CLASS_NO_COPY(SignalEngine);

	bool SetSigaction(uint32_t pid, int32_t sig, const KernelSigaction& act, KernelSigaction* old_act_out);
	bool GetSigaction(uint32_t pid, int32_t sig, KernelSigaction* act_out) const;

	bool SendSignal(uint32_t target_pid, uint32_t target_tid, int32_t sig, const KernelSiginfo* info = nullptr);
	bool SetThreadSigmask(uint32_t tid, int32_t how, uint64_t set, uint64_t* old_set_out);
	uint64_t GetPendingSignals(uint32_t tid) const;

	bool WaitSignal(uint32_t tid, uint64_t wait_mask, KernelSiginfo* info_out, uint64_t timeout_us = 0);

private:
	mutable std::mutex                                                  m_mutex;
	std::unordered_map<uint32_t, std::unordered_map<int32_t, KernelSigaction>> m_process_actions;
	std::unordered_map<uint32_t, uint64_t>                             m_thread_sigmasks;
	std::unordered_map<uint32_t, uint64_t>                             m_thread_pending;
	std::unordered_map<uint32_t, std::vector<KernelSiginfo>>           m_thread_queue;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_SIGNAL_ENGINE_H
