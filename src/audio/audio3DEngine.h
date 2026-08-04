// audio3DEngine.h
//
// 3D Spatial Audio Engine (VBAP / Ambisonics / 7.1.4 Spatial Positioning) for PS5 Audio Subsystem.

#ifndef AUDIO_AUDIO_3D_ENGINE_H
#define AUDIO_AUDIO_3D_ENGINE_H

#include "audio/audioVoiceMixer.h"
#include "common/common.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <vector>

namespace Libs::Audio {

struct AudioPosition3D {
	float x         = 0.0f; // Left (-) to Right (+)
	float y         = 0.0f; // Back (-) to Front (+)
	float z         = 0.0f; // Down (-) to Up (+)
	float spread    = 0.0f; // 0.0 (point source) to 1.0 (ambient)
	float min_dist  = 1.0f;
	float max_dist  = 50.0f;
};

class Audio3DEngine {
public:
	Audio3DEngine() = default;
	~Audio3DEngine() = default;

	KYTY_CLASS_NO_COPY(Audio3DEngine);

	static void ComputeVbapCoefficients(const AudioPosition3D& pos, float* out_coefficients, uint32_t channel_count = 12, bool height_aware = true);
	static void ComputeAmbisonicsCoefficients(uint32_t order, float azimuth, float elevation, float* out_coefficients, uint32_t channel_count);

	bool SetVoicePosition(VoiceMixer& mixer, uint32_t voice_id, const AudioPosition3D& pos, bool height_aware = true);
	void Set3DLatencyUs(uint32_t latency_us);

	[[nodiscard]] uint32_t Get3DLatencyUs() const noexcept { return m_latency_us; }

private:
	static float CalculateDistanceAttenuation(float distance, float min_dist, float max_dist);

	uint32_t           m_latency_us = 10000; // 10ms default
	mutable std::mutex m_mutex;
};

} // namespace Libs::Audio

#endif // AUDIO_AUDIO_3D_ENGINE_H
