// ps5Sync.cpp
//
// Kernel Synchronization Primitives for Phase N PS5 Kernel Emulation.

#include "kernel/ps5Sync.h"

namespace Libs::Kernel::Ps5 {

// ─── KernelMutex ─────────────────────────────────────────────────────────────

void KernelMutex::Lock() {
	m_mutex.lock();
}

bool KernelMutex::TryLock() {
	return m_mutex.try_lock();
}

bool KernelMutex::TimedLock(uint64_t timeout_us) {
	if (timeout_us == 0) {
		return TryLock();
	}
	// Simulated timed lock
	Lock();
	return true;
}

void KernelMutex::Unlock() {
	m_mutex.unlock();
}

// ─── KernelRwLock ────────────────────────────────────────────────────────────

void KernelRwLock::LockRead() {
	m_rw_mutex.lock_shared();
}

void KernelRwLock::UnlockRead() {
	m_rw_mutex.unlock_shared();
}

void KernelRwLock::LockWrite() {
	m_rw_mutex.lock();
}

void KernelRwLock::UnlockWrite() {
	m_rw_mutex.unlock();
}

// ─── KernelSemaphore ─────────────────────────────────────────────────────────

KernelSemaphore::KernelSemaphore(const std::string& name, int32_t init_count, int32_t max_count)
    : m_name(name), m_count(init_count), m_max_count(max_count) {}

bool KernelSemaphore::Wait(uint64_t timeout_us) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (timeout_us == 0) {
		m_cv.wait(lock, [this]() { return m_count > 0; });
		m_count--;
		return true;
	} else {
		bool ok = m_cv.wait_for(lock, std::chrono::microseconds(timeout_us), [this]() { return m_count > 0; });
		if (ok) {
			m_count--;
		}
		return ok;
	}
}

bool KernelSemaphore::Signal(int32_t count) {
	std::unique_lock<std::mutex> lock(m_mutex);
	if (m_count + count > m_max_count) {
		return false;
	}
	m_count += count;
	for (int i = 0; i < count; ++i) {
		m_cv.notify_one();
	}
	return true;
}

int32_t KernelSemaphore::GetCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_count;
}

// ─── KernelCond ──────────────────────────────────────────────────────────────

bool KernelCond::Wait(KernelMutex& mutex, uint64_t timeout_us) {
	mutex.Unlock();
	std::mutex local_m;
	std::unique_lock<std::mutex> lock(local_m);
	bool res = true;
	if (timeout_us == 0) {
		m_cv.wait(lock);
	} else {
		res = (m_cv.wait_for(lock, std::chrono::microseconds(timeout_us)) == std::cv_status::no_timeout);
	}
	mutex.Lock();
	return res;
}

void KernelCond::Signal() {
	m_cv.notify_one();
}

void KernelCond::Broadcast() {
	m_cv.notify_all();
}

} // namespace Libs::Kernel::Ps5
