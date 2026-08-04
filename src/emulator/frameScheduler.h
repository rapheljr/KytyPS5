// frameScheduler.h
//
// Frame Scheduling & Pacing Infrastructure for Phase O Full Integration.

#ifndef EMULATOR_FRAME_SCHEDULER_H
#define EMULATOR_FRAME_SCHEDULER_H

#include "common/common.h"

#include <chrono>
#include <cstdint>

namespace Emulator {

struct FrameTimingStats {
	uint64_t target_framerate  = 60;
	double   target_frame_ms   = 16.666;
	double   actual_frame_ms   = 0.0;
	double   fps               = 0.0;
	uint64_t total_frames       = 0;
};

class FrameScheduler {
public:
	explicit FrameScheduler(uint32_t target_fps = 60);
	~FrameScheduler() = default;

	KYTY_CLASS_NO_COPY(FrameScheduler);

	void SetTargetFramerate(uint32_t target_fps) noexcept;
	[[nodiscard]] uint32_t GetTargetFramerate() const noexcept { return static_cast<uint32_t>(m_stats.target_framerate); }

	void BeginFrame();
	void EndFrameAndPace();

	[[nodiscard]] const FrameTimingStats& GetStats() const noexcept { return m_stats; }

private:
	FrameTimingStats                                   m_stats{};
	std::chrono::high_resolution_clock::time_point    m_frame_start_time;
	std::chrono::high_resolution_clock::time_point    m_last_pacing_time;
};

} // namespace Emulator

#endif // EMULATOR_FRAME_SCHEDULER_H
