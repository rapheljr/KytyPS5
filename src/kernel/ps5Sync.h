// ps5Sync.h
//
// Kernel Synchronization Primitives for Phase N PS5 Kernel Emulation.

#ifndef KERNEL_PS5_SYNC_H
#define KERNEL_PS5_SYNC_H

#include "common/common.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace Libs::Kernel::Ps5 {

class KernelMutex {
public:
	explicit KernelMutex(const std::string& name = "", bool recursive = false)
	    : m_name(name), m_recursive(recursive) {}
	~KernelMutex() = default;

	KYTY_CLASS_NO_COPY(KernelMutex);

	void Lock();
	bool TryLock();
	bool TimedLock(uint64_t timeout_us);
	void Unlock();

private:
	std::string m_name;
	bool        m_recursive = false;
	std::mutex  m_mutex;
};

class KernelRwLock {
public:
	explicit KernelRwLock(const std::string& name = "") : m_name(name) {}
	~KernelRwLock() = default;

	KYTY_CLASS_NO_COPY(KernelRwLock);

	void LockRead();
	void UnlockRead();
	void LockWrite();
	void UnlockWrite();

private:
	std::string        m_name;
	std::shared_mutex m_rw_mutex;
};

class KernelSemaphore {
public:
	explicit KernelSemaphore(const std::string& name, int32_t init_count, int32_t max_count);
	~KernelSemaphore() = default;

	KYTY_CLASS_NO_COPY(KernelSemaphore);

	bool Wait(uint64_t timeout_us = 0);
	bool Signal(int32_t count = 1);
	[[nodiscard]] int32_t GetCount() const noexcept;

private:
	std::string             m_name;
	int32_t                 m_count = 0;
	int32_t                 m_max_count = 1;
	mutable std::mutex      m_mutex;
	std::condition_variable m_cv;
};

class KernelCond {
public:
	explicit KernelCond(const std::string& name = "") : m_name(name) {}
	~KernelCond() = default;

	KYTY_CLASS_NO_COPY(KernelCond);

	bool Wait(KernelMutex& mutex, uint64_t timeout_us = 0);
	void Signal();
	void Broadcast();

private:
	std::string             m_name;
	std::condition_variable m_cv;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_SYNC_H
