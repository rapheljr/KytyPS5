// ps5Kernel.cpp
//
// Syscall Dispatcher, Thread Manager & Process Manager Implementation for PS5 Kernel Emulation.

#include "kernel/ps5Kernel.h"

#include <cstring>

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
	RegisterAllFreeBSDSyscalls();
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

void SyscallDispatcher::RegisterAllFreeBSDSyscalls() {
	// Process Management
	RegisterSyscall(1, [](uint64_t exit_code, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_exit
	});
	RegisterSyscall(2, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 100; // sys_fork -> return child PID
	});
	RegisterSyscall(7, [](uint64_t pid, uint64_t status_ptr, uint64_t options, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (status_ptr != 0) {
			*reinterpret_cast<int32_t*>(status_ptr) = 0; // sys_wait4
		}
		return static_cast<int64_t>(pid > 0 ? pid : 1);
	});
	RegisterSyscall(20, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 1; // sys_getpid
	});
	RegisterSyscall(24, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_getuid (root)
	});
	RegisterSyscall(25, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_geteuid
	});
	RegisterSyscall(37, [](uint64_t pid, uint64_t sig, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_kill
	});
	RegisterSyscall(39, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_getppid
	});
	RegisterSyscall(47, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_getgid
	});
	RegisterSyscall(50, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_getegid
	});
	RegisterSyscall(59, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_execve
	});

	// Thread Management (FreeBSD thr_*)
	RegisterSyscall(430, [](uint64_t param_ptr, uint64_t param_size, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_thr_create
	});
	RegisterSyscall(431, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_thr_exit
	});
	RegisterSyscall(432, [](uint64_t id_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (id_ptr != 0) {
			*reinterpret_cast<int64_t*>(id_ptr) = 1; // sys_thr_self
		}
		return 0;
	});
	RegisterSyscall(433, [](uint64_t id, uint64_t sig, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_thr_kill
	});
	RegisterSyscall(464, [](uint64_t id, uint64_t name_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_thr_set_name
	});
	RegisterSyscall(586, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 1; // sys_gettid
	});

	// Scheduler
	RegisterSyscall(327, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_setparam
	RegisterSyscall(328, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_getparam
	RegisterSyscall(329, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_setscheduler
	RegisterSyscall(330, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_getscheduler
	RegisterSyscall(331, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_yield
	RegisterSyscall(332, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 255; }); // sys_sched_get_priority_max
	RegisterSyscall(333, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sched_get_priority_min
	RegisterSyscall(487, [](uint64_t, uint64_t, uint64_t, uint64_t mask_ptr, uint64_t, uint64_t) -> int64_t {
		if (mask_ptr != 0) {
			*reinterpret_cast<uint64_t*>(mask_ptr) = 0xFFFFFFFF; // sys_cpuset_getaffinity
		}
		return 0;
	});
	RegisterSyscall(488, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_cpuset_setaffinity

	// Virtual Memory
	RegisterSyscall(73, [](uint64_t addr, uint64_t len, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_munmap
	RegisterSyscall(74, [](uint64_t addr, uint64_t len, uint64_t prot, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_mprotect
	RegisterSyscall(75, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_madvise
	RegisterSyscall(203, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_mlock
	RegisterSyscall(204, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_munlock
	RegisterSyscall(477, [](uint64_t addr, uint64_t len, uint64_t prot, uint64_t flags, uint64_t fd, uint64_t offset) -> int64_t {
		return static_cast<int64_t>(addr != 0 ? addr : 0x10000000ULL); // sys_mmap
	});

	// Shared Memory & IPC
	RegisterSyscall(42, [](uint64_t fds_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (fds_ptr != 0) {
			auto* fds = reinterpret_cast<int32_t*>(fds_ptr);
			fds[0] = 50;
			fds[1] = 51; // sys_pipe
		}
		return 0;
	});
	RegisterSyscall(135, [](uint64_t domain, uint64_t type, uint64_t protocol, uint64_t fds_ptr, uint64_t, uint64_t) -> int64_t {
		if (fds_ptr != 0) {
			auto* fds = reinterpret_cast<int32_t*>(fds_ptr);
			fds[0] = 52;
			fds[1] = 53; // sys_socketpair
		}
		return 0;
	});
	RegisterSyscall(482, [](uint64_t path_ptr, uint64_t flags, uint64_t mode, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 20; // sys_shm_open (fd = 20)
	});
	RegisterSyscall(483, [](uint64_t path_ptr, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 0; // sys_shm_unlink
	});
	RegisterSyscall(542, [](uint64_t fds_ptr, uint64_t flags, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (fds_ptr != 0) {
			auto* fds = reinterpret_cast<int32_t*>(fds_ptr);
			fds[0] = 54;
			fds[1] = 55; // sys_pipe2
		}
		return 0;
	});

	// Clocks & Timers
	RegisterSyscall(116, [](uint64_t tv_ptr, uint64_t tz_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (tv_ptr != 0) {
			auto* tv = reinterpret_cast<KernelTimeval*>(tv_ptr);
			tv->tv_sec = 1600000000;
			tv->tv_usec = 0;
		}
		return 0; // sys_gettimeofday
	});
	RegisterSyscall(232, [](uint64_t clock_id, uint64_t ts_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (ts_ptr != 0) {
			auto ts = KernelTimerManager::GetClockTime(static_cast<int32_t>(clock_id));
			*reinterpret_cast<KernelTimespec*>(ts_ptr) = ts; // sys_clock_gettime
		}
		return 0;
	});
	RegisterSyscall(233, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_clock_settime
	RegisterSyscall(234, [](uint64_t clock_id, uint64_t res_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (res_ptr != 0) {
			auto* ts = reinterpret_cast<KernelTimespec*>(res_ptr);
			ts->tv_sec  = 0;
			ts->tv_nsec = 1; // 1 ns resolution (sys_clock_getres)
		}
		return 0;
	});
	RegisterSyscall(236, [](uint64_t timerid, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_ktimer_delete
	RegisterSyscall(237, [](uint64_t timerid, uint64_t flags, uint64_t value_ptr, uint64_t ovalue_ptr, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_ktimer_settime
	RegisterSyscall(238, [](uint64_t timerid, uint64_t value_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_ktimer_gettime
	RegisterSyscall(240, [](uint64_t req_ptr, uint64_t rem_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (req_ptr != 0) {
			auto* ts = reinterpret_cast<KernelTimespec*>(req_ptr);
			uint64_t nsec = static_cast<uint64_t>(ts->tv_sec) * 1'000'000'000ULL + static_cast<uint64_t>(ts->tv_nsec);
			KernelTimerManager::SleepNanoseconds(nsec);
		}
		return 0; // sys_nanosleep
	});
	RegisterSyscall(585, [](uint64_t clock_id, uint64_t evp_ptr, uint64_t timerid_ptr, uint64_t, uint64_t, uint64_t) -> int64_t {
		if (timerid_ptr != 0) {
			*reinterpret_cast<int32_t*>(timerid_ptr) = 10; // sys_ktimer_create
		}
		return 0;
	});

	// Signals
	RegisterSyscall(340, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigprocmask
	RegisterSyscall(341, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigsuspend
	RegisterSyscall(343, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigpending
	RegisterSyscall(345, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigtimedwait
	RegisterSyscall(346, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigwaitinfo
	RegisterSyscall(416, [](uint64_t sig, uint64_t act_ptr, uint64_t oact_ptr, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigaction
	RegisterSyscall(456, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_sigqueue

	// Synchronization (_umtx_op)
	RegisterSyscall(454, [](uint64_t obj, uint64_t op, uint64_t val, uint64_t uaddr1, uint64_t uaddr2, uint64_t) -> int64_t {
		return 0; // sys_umtx_op
	});

	// Event Queue (kqueue/kevent)
	RegisterSyscall(362, [](uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t {
		return 10; // sys_kqueue (fd = 10)
	});
	RegisterSyscall(363, [](uint64_t fd, uint64_t changelist_ptr, uint64_t nchanges, uint64_t eventlist_ptr, uint64_t nevents, uint64_t timeout_ptr) -> int64_t {
		return 0; // sys_kevent
	});

	// File I/O & File Descriptors
	RegisterSyscall(3, [](uint64_t fd, uint64_t buf, uint64_t count, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_read
	RegisterSyscall(4, [](uint64_t fd, uint64_t buf, uint64_t count, uint64_t, uint64_t, uint64_t) -> int64_t { return static_cast<int64_t>(count); }); // sys_write
	RegisterSyscall(5, [](uint64_t path_ptr, uint64_t flags, uint64_t mode, uint64_t, uint64_t, uint64_t) -> int64_t { return 11; }); // sys_open (fd = 11)
	RegisterSyscall(6, [](uint64_t fd, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_close
	RegisterSyscall(54, [](uint64_t fd, uint64_t cmd, uint64_t arg, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_ioctl
	RegisterSyscall(65, [](uint64_t addr, uint64_t len, uint64_t flags, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_msync
	RegisterSyscall(92, [](uint64_t fd, uint64_t cmd, uint64_t arg, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_fcntl
	RegisterSyscall(116, [](uint64_t tp_ptr, uint64_t tzp_ptr, uint64_t, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_gettimeofday
	RegisterSyscall(202, [](uint64_t name_ptr, uint64_t namelen, uint64_t oldp, uint64_t oldlenp, uint64_t newp, uint64_t newlen) -> int64_t { return 0; }); // sys_sysctl
	RegisterSyscall(466, [](uint64_t function, uint64_t lwpid, uint64_t rtp_ptr, uint64_t, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_rtprio_thread
	RegisterSyscall(486, [](uint64_t level, uint64_t which, uint64_t id, uint64_t setid_ptr, uint64_t, uint64_t) -> int64_t { return 0; }); // sys_cpuset_getid
}

} // namespace Libs::Kernel::Ps5
