// frameScheduler.cpp
//
// Frame Scheduling & Pacing Infrastructure for Phase O Full Integration.

#include "emulator/frameScheduler.h"

#include <thread>

namespace Emulator {

FrameScheduler::FrameScheduler(uint32_t target_fps) {
	SetTargetFramerate(target_fps);
	m_last_pacing_time = std::chrono::high_resolution_clock::now();
}

void FrameScheduler::SetTargetFramerate(uint32_t target_fps) noexcept {
	if (target_fps == 0) target_fps = 60;
	m_stats.target_framerate = target_fps;
	m_stats.target_frame_ms  = 1000.0 / target_fps;
}

void FrameScheduler::BeginFrame() {
	m_frame_start_time = std::chrono::high_resolution_clock::now();
}

void FrameScheduler::EndFrameAndPace() {
	auto now = std::chrono::high_resolution_clock::now();
	double elapsed_ms = std::chrono::duration<double, std::milli>(now - m_frame_start_time).count();

	if (elapsed_ms < m_stats.target_frame_ms) {
		double sleep_ms = m_stats.target_frame_ms - elapsed_ms;
		std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(sleep_ms));
	}

	auto end_time = std::chrono::high_resolution_clock::now();
	double actual_ms = std::chrono::duration<double, std::milli>(end_time - m_last_pacing_time).count();
	m_last_pacing_time = end_time;

	m_stats.actual_frame_ms = actual_ms;
	m_stats.fps             = (actual_ms > 0.0) ? (1000.0 / actual_ms) : 0.0;
	m_stats.total_frames++;
}

} // namespace Emulator
