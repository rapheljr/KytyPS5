// tempest3DAudioEngine.h
//
// Tempest 3D Audio HRTF Spatial DSP Engine for KytyPS5.
// Simulates PS5 3D Ambisonics, Binaural HRTF convolution, and 7.1.4 surround rendering.

#ifndef AUDIO_TEMPEST_3D_AUDIO_ENGINE_H
#define AUDIO_TEMPEST_3D_AUDIO_ENGINE_H

#include "common/common.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Audio {

struct SpatialPosition3D {
	float x = 0.0f; // Left (-) to Right (+)
	float y = 0.0f; // Down (-) to Up (+)
	float z = 0.0f; // Back (-) to Front (+)
};

struct Spatial3DSoundEmitter {
	uint32_t          id = 0;
	SpatialPosition3D position{};
	float             volume     = 1.0f;
	float             pitch      = 1.0f;
	bool              is_playing = true;
};

struct Tempest3DStats {
	uint32_t total_emitters_rendered  = 0;
	uint64_t total_samples_convolved  = 0;
	double   hrtf_processing_time_us  = 0.0;
};

class Tempest3DAudioEngine {
public:
	Tempest3DAudioEngine();
	~Tempest3DAudioEngine() = default;

	KYTY_CLASS_NO_COPY(Tempest3DAudioEngine);

	bool Initialize(uint32_t sample_rate = 48000, uint32_t buffer_frames = 512);
	void Shutdown();

	/// Register or update a 3D spatial emitter
	uint32_t CreateEmitter(const SpatialPosition3D& pos, float volume = 1.0f);
	void SetEmitterPosition(uint32_t emitter_id, const SpatialPosition3D& pos);

	/// Process mono sound source into binaural stereo HRTF stream (L, R)
	void ProcessBinauralHrtf(uint32_t emitter_id, const float* mono_in, float* left_out, float* right_out, size_t frames);

	/// Process 7.1.4 12-channel surround output
	void ProcessSurround714(uint32_t emitter_id, const float* mono_in, float* const* channel_outputs, size_t frames);

	[[nodiscard]] const Tempest3DStats& GetStats() const noexcept { return m_stats; }
	[[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

private:
	uint32_t                        m_sample_rate   = 48000;
	uint32_t                        m_buffer_frames = 512;
	bool                            m_initialized   = false;
	std::vector<Spatial3DSoundEmitter> m_emitters;
	Tempest3DStats                  m_stats{};
};

} // namespace Audio

#endif // AUDIO_TEMPEST_3D_AUDIO_ENGINE_H
