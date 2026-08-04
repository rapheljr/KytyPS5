#include "graphics/host_gpu/renderer/backend/metalSync.h"

namespace Libs::Graphics::HostGpu::Metal {

MetalFence::MetalFence()  = default;
MetalFence::~MetalFence() = default;

bool MetalFence::Initialize(void* /*device_handle*/) { return false; }
void MetalFence::Reset() {}
void MetalFence::UpdateInComputeEncoder(void* /*compute_encoder_handle*/) {}
void MetalFence::WaitForInComputeEncoder(void* /*compute_encoder_handle*/) {}
void MetalFence::UpdateInRenderEncoder(void* /*render_encoder_handle*/) {}
void MetalFence::WaitForInRenderEncoder(void* /*render_encoder_handle*/) {}

MetalEvent::MetalEvent()  = default;
MetalEvent::~MetalEvent() = default;

bool MetalEvent::Initialize(void* /*device_handle*/, bool /*shared*/) { return false; }
void MetalEvent::Reset() {}
void MetalEvent::SignalOnCommandBuffer(void* /*command_buffer_handle*/, uint64_t /*value*/) {}
void MetalEvent::WaitOnCommandBuffer(void* /*command_buffer_handle*/, uint64_t /*value*/) {}
uint64_t MetalEvent::GetSignaledValue() const { return 0; }
void MetalEvent::SignalFromHost(uint64_t /*value*/) {}

void MetalResourceHazardTracker::TrackResourceAccess(uint64_t /*resource_id*/, MetalResourceAccess /*access*/,
                                                       bool& out_raw_hazard, bool& out_war_hazard, bool& out_waw_hazard) {
	out_raw_hazard = false;
	out_war_hazard = false;
	out_waw_hazard = false;
}

void MetalResourceHazardTracker::Reset() {}

MetalFrameSync::MetalFrameSync(size_t max_in_flight) { SetMaxFramesInFlight(max_in_flight); }
MetalFrameSync::~MetalFrameSync() = default;

void MetalFrameSync::SetMaxFramesInFlight(size_t max_in_flight) { m_max_in_flight = max_in_flight; }
void MetalFrameSync::BeginFrame() {}
void MetalFrameSync::EndFrame(void* /*command_buffer_handle*/) {
	m_frame_index.fetch_add(1, std::memory_order_relaxed);
	m_total_frames.fetch_add(1, std::memory_order_relaxed);
}

} // namespace Libs::Graphics::HostGpu::Metal
