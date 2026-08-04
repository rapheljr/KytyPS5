// kernelTimer.cpp
//
// High-Resolution Timers, POSIX Clocks & Event Queue Dispatch Implementation.

#include "kernel/kernelTimer.h"

#include <thread>

namespace Libs::Kernel::Ps5 {

void KernelTimer::SetTime(const KernelITimerspec& spec) {
	m_spec   = spec;
	m_active = (spec.it_value.tv_sec > 0 || spec.it_value.tv_nsec > 0);
	if (m_active) {
		auto delay = std::chrono::seconds(spec.it_value.tv_sec) + std::chrono::nanoseconds(spec.it_value.tv_nsec);
		m_expire_time = std::chrono::steady_clock::now() + delay;
	}
}

void KernelTimer::GetTime(KernelITimerspec* spec_out) const {
	if (spec_out) {
		*spec_out = m_spec;
	}
}

void KernelTimer::Cancel() {
	m_active = false;
	m_spec.it_value.tv_sec  = 0;
	m_spec.it_value.tv_nsec = 0;
}

// ─── KernelTimerManager ───────────────────────────────────────────────────────

KernelTimerManager::KernelTimerManager() = default;

KernelTimerManager::~KernelTimerManager() = default;

KernelTimespec KernelTimerManager::GetClockTime(int32_t clock_id) {
	KernelTimespec ts{};
	if (clock_id == KERNEL_CLOCK_REALTIME) {
		auto now = std::chrono::system_clock::now().time_since_epoch();
		ts.tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() % 1'000'000'000;
	} else {
		auto now = std::chrono::steady_clock::now().time_since_epoch();
		ts.tv_sec  = std::chrono::duration_cast<std::chrono::seconds>(now).count();
		ts.tv_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count() % 1'000'000'000;
	}
	return ts;
}

void KernelTimerManager::SleepNanoseconds(uint64_t nsec) {
	std::this_thread::sleep_for(std::chrono::nanoseconds(nsec));
}

int32_t KernelTimerManager::CreateTimer(int32_t clock_id, uint32_t owner_pid) {
	std::lock_guard<std::mutex> lock(m_mutex);
	int32_t timer_id = m_next_timer_id++;
	auto timer = std::make_shared<KernelTimer>(clock_id, owner_pid);
	timer->SetHandle(timer_id);
	m_timers[timer_id] = timer;
	return timer_id;
}

bool KernelTimerManager::SetTimerTime(int32_t timer_id, const KernelITimerspec& spec, KernelITimerspec* old_spec_out) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_timers.find(timer_id);
	if (it == m_timers.end()) {
		return false;
	}

	if (old_spec_out) {
		it->second->GetTime(old_spec_out);
	}
	it->second->SetTime(spec);
	return true;
}

bool KernelTimerManager::GetTimerTime(int32_t timer_id, KernelITimerspec* spec_out) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_timers.find(timer_id);
	if (it == m_timers.end()) {
		return false;
	}
	it->second->GetTime(spec_out);
	return true;
}

bool KernelTimerManager::DeleteTimer(int32_t timer_id) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_timers.find(timer_id);
	if (it == m_timers.end()) {
		return false;
	}
	it->second->Cancel();
	m_timers.erase(it);
	return true;
}

size_t KernelTimerManager::GetActiveTimerCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_timers.size();
}

} // namespace Libs::Kernel::Ps5
