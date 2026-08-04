// kernelTimer.h
//
// High-Resolution Timers, POSIX Clocks & Event Queue Dispatch for PS5 Kernel Emulation.

#ifndef KERNEL_TIMER_H
#define KERNEL_TIMER_H

#include "common/common.h"
#include "kernel/kernelObject.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace Libs::Kernel::Ps5 {

constexpr int32_t KERNEL_CLOCK_REALTIME         = 0;
constexpr int32_t KERNEL_CLOCK_MONOTONIC        = 4;
constexpr int32_t KERNEL_CLOCK_MONOTONIC_FAST   = 12;
constexpr int32_t KERNEL_CLOCK_PROCESS_CPUTIME  = 14;

struct KernelTimespec {
	int64_t tv_sec  = 0;
	int64_t tv_nsec = 0;
};

struct KernelTimeval {
	int64_t tv_sec  = 0;
	int64_t tv_usec = 0;
};

struct KernelITimerspec {
	KernelTimespec it_interval;
	KernelTimespec it_value;
};

class KernelTimer : public KernelObject {
public:
	KernelTimer(int32_t clock_id, uint32_t owner_pid = 1)
	    : KernelObject(KernelObjectType::Timer, "KernelTimer", owner_pid), m_clock_id(clock_id) {}
	~KernelTimer() override = default;

	KYTY_CLASS_NO_COPY(KernelTimer);

	void SetTime(const KernelITimerspec& spec);
	void GetTime(KernelITimerspec* spec_out) const;
	void Cancel();

	[[nodiscard]] int32_t GetClockId() const noexcept { return m_clock_id; }
	[[nodiscard]] bool IsActive() const noexcept { return m_active; }

private:
	int32_t                          m_clock_id = KERNEL_CLOCK_MONOTONIC;
	bool                             m_active   = false;
	KernelITimerspec                 m_spec{};
	std::chrono::steady_clock::time_point m_expire_time;
};

using KernelTimerRef = std::shared_ptr<KernelTimer>;

class KernelTimerManager {
public:
	KernelTimerManager();
	~KernelTimerManager();

	KYTY_CLASS_NO_COPY(KernelTimerManager);

	static KernelTimespec GetClockTime(int32_t clock_id);
	static void SleepNanoseconds(uint64_t nsec);

	int32_t CreateTimer(int32_t clock_id, uint32_t owner_pid = 1);
	bool SetTimerTime(int32_t timer_id, const KernelITimerspec& spec, KernelITimerspec* old_spec_out);
	bool GetTimerTime(int32_t timer_id, KernelITimerspec* spec_out) const;
	bool DeleteTimer(int32_t timer_id);

	[[nodiscard]] size_t GetActiveTimerCount() const;

private:
	mutable std::mutex                             m_mutex;
	int32_t                                        m_next_timer_id = 1;
	std::unordered_map<int32_t, KernelTimerRef>    m_timers;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_TIMER_H
