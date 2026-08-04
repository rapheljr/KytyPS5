#ifndef KYTY_GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SYNC_H
#define KYTY_GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SYNC_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics::HostGpu::Metal {

enum class MetalResourceAccess {
	None,
	Read,
	Write,
	ReadWrite
};

class MetalFence {
public:
	MetalFence();
	~MetalFence();

	bool Initialize(void* device_handle);
	void Reset();

	void UpdateInComputeEncoder(void* compute_encoder_handle);
	void WaitForInComputeEncoder(void* compute_encoder_handle);
	void UpdateInRenderEncoder(void* render_encoder_handle);
	void WaitForInRenderEncoder(void* render_encoder_handle);

	void* GetNativeFence() const { return m_fence; }
	bool IsValid() const { return m_fence != nullptr; }

private:
	void* m_fence = nullptr;
};

class MetalEvent {
public:
	MetalEvent();
	~MetalEvent();

	bool Initialize(void* device_handle, bool shared = false);
	void Reset();

	void SignalOnCommandBuffer(void* command_buffer_handle, uint64_t value);
	void WaitOnCommandBuffer(void* command_buffer_handle, uint64_t value);
	uint64_t GetSignaledValue() const;
	void SignalFromHost(uint64_t value);

	void* GetNativeEvent() const { return m_event; }
	bool IsValid() const { return m_event != nullptr; }
	bool IsShared() const { return m_is_shared; }

private:
	void* m_event     = nullptr;
	bool  m_is_shared = false;
};

class MetalResourceHazardTracker {
public:
	MetalResourceHazardTracker()  = default;
	~MetalResourceHazardTracker() = default;

	void TrackResourceAccess(uint64_t resource_id, MetalResourceAccess access,
	                         bool& out_raw_hazard, bool& out_war_hazard, bool& out_waw_hazard);
	void Reset();

private:
	std::mutex                                          m_mutex;
	std::unordered_map<uint64_t, MetalResourceAccess>   m_resource_states;
};

class MetalFrameSync {
public:
	explicit MetalFrameSync(size_t max_in_flight = 3);
	~MetalFrameSync();

	void SetMaxFramesInFlight(size_t max_in_flight);
	size_t GetMaxFramesInFlight() const { return m_max_in_flight; }

	void BeginFrame();
	void EndFrame(void* command_buffer_handle);

	size_t GetCurrentFrameIndex() const { return m_frame_index.load(); }
	uint64_t GetTotalFramesPresented() const { return m_total_frames.load(); }

private:
	size_t              m_max_in_flight = 3;
	std::atomic<size_t>   m_frame_index{0};
	std::atomic<uint64_t> m_total_frames{0};
	void*               m_semaphores[3] = {};
};

} // namespace Libs::Graphics::HostGpu::Metal

#endif // KYTY_GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SYNC_H
