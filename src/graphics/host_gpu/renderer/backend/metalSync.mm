#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "graphics/host_gpu/renderer/backend/metalSync.h"

#include <algorithm>

namespace Libs::Graphics::HostGpu::Metal {

MetalFence::MetalFence() = default;

MetalFence::~MetalFence() {
	Reset();
}

bool MetalFence::Initialize(void* device_handle) {
	Reset();
	if (!device_handle) {
		return false;
	}

	id<MTLDevice> device = (__bridge id<MTLDevice>)device_handle;
	id<MTLFence> fence  = [device newFence];
	if (!fence) {
		return false;
	}

	m_fence = (void*)CFBridgingRetain(fence);
	return true;
}

void MetalFence::Reset() {
	if (m_fence) {
		id<MTLFence> fence = (id<MTLFence>)CFBridgingRelease(m_fence);
		(void)fence;
		m_fence = nullptr;
	}
}

void MetalFence::UpdateInComputeEncoder(void* compute_encoder_handle) {
	if (!m_fence || !compute_encoder_handle) return;
	id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)compute_encoder_handle;
	id<MTLFence>                 fence   = (__bridge id<MTLFence>)m_fence;
	[encoder updateFence:fence];
}

void MetalFence::WaitForInComputeEncoder(void* compute_encoder_handle) {
	if (!m_fence || !compute_encoder_handle) return;
	id<MTLComputeCommandEncoder> encoder = (__bridge id<MTLComputeCommandEncoder>)compute_encoder_handle;
	id<MTLFence>                 fence   = (__bridge id<MTLFence>)m_fence;
	[encoder waitForFence:fence];
}

void MetalFence::UpdateInRenderEncoder(void* render_encoder_handle) {
	if (!m_fence || !render_encoder_handle) return;
	id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)render_encoder_handle;
	id<MTLFence>                fence   = (__bridge id<MTLFence>)m_fence;
	[encoder updateFence:fence];
}

void MetalFence::WaitForInRenderEncoder(void* render_encoder_handle) {
	if (!m_fence || !render_encoder_handle) return;
	id<MTLRenderCommandEncoder> encoder = (__bridge id<MTLRenderCommandEncoder>)render_encoder_handle;
	id<MTLFence>                fence   = (__bridge id<MTLFence>)m_fence;
	[encoder waitForFence:fence];
}

MetalEvent::MetalEvent() = default;

MetalEvent::~MetalEvent() {
	Reset();
}

bool MetalEvent::Initialize(void* device_handle, bool shared) {
	Reset();
	if (!device_handle) {
		return false;
	}

	id<MTLDevice> device = (__bridge id<MTLDevice>)device_handle;
	if (shared) {
		id<MTLSharedEvent> event = [device newSharedEvent];
		if (!event) return false;
		m_event     = (void*)CFBridgingRetain(event);
		m_is_shared = true;
	} else {
		id<MTLEvent> event = [device newEvent];
		if (!event) return false;
		m_event     = (void*)CFBridgingRetain(event);
		m_is_shared = false;
	}

	return m_event != nullptr;
}

void MetalEvent::Reset() {
	if (m_event) {
		CFBridgingRelease(m_event);
		m_event     = nullptr;
		m_is_shared = false;
	}
}

void MetalEvent::SignalOnCommandBuffer(void* command_buffer_handle, uint64_t value) {
	if (!m_event || !command_buffer_handle) return;
	id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command_buffer_handle;
	if (m_is_shared) {
		id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)m_event;
		[cb encodeSignalEvent:event value:value];
	} else {
		id<MTLEvent> event = (__bridge id<MTLEvent>)m_event;
		[cb encodeSignalEvent:event value:value];
	}
}

void MetalEvent::WaitOnCommandBuffer(void* command_buffer_handle, uint64_t value) {
	if (!m_event || !command_buffer_handle) return;
	id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command_buffer_handle;
	if (m_is_shared) {
		id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)m_event;
		[cb encodeWaitForEvent:event value:value];
	} else {
		id<MTLEvent> event = (__bridge id<MTLEvent>)m_event;
		[cb encodeWaitForEvent:event value:value];
	}
}

uint64_t MetalEvent::GetSignaledValue() const {
	if (m_is_shared && m_event) {
		id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)m_event;
		return event.signaledValue;
	}
	return 0;
}

void MetalEvent::SignalFromHost(uint64_t value) {
	if (m_is_shared && m_event) {
		id<MTLSharedEvent> event = (__bridge id<MTLSharedEvent>)m_event;
		event.signaledValue      = value;
	}
}

void MetalResourceHazardTracker::TrackResourceAccess(uint64_t resource_id, MetalResourceAccess access,
                                                       bool& out_raw_hazard, bool& out_war_hazard, bool& out_waw_hazard) {
	std::lock_guard<std::mutex> lock(m_mutex);

	out_raw_hazard = false;
	out_war_hazard = false;
	out_waw_hazard = false;

	auto it = m_resource_states.find(resource_id);
	if (it != m_resource_states.end()) {
		MetalResourceAccess prev = it->second;
		if (prev == MetalResourceAccess::Write || prev == MetalResourceAccess::ReadWrite) {
			if (access == MetalResourceAccess::Read || access == MetalResourceAccess::ReadWrite) {
				out_raw_hazard = true;
			}
			if (access == MetalResourceAccess::Write || access == MetalResourceAccess::ReadWrite) {
				out_waw_hazard = true;
			}
		} else if (prev == MetalResourceAccess::Read) {
			if (access == MetalResourceAccess::Write || access == MetalResourceAccess::ReadWrite) {
				out_war_hazard = true;
			}
		}
	}

	m_resource_states[resource_id] = access;
}

void MetalResourceHazardTracker::Reset() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_resource_states.clear();
}

MetalFrameSync::MetalFrameSync(size_t max_in_flight) {
	SetMaxFramesInFlight(max_in_flight);
}

MetalFrameSync::~MetalFrameSync() {
	for (size_t i = 0; i < 3; ++i) {
		if (m_semaphores[i]) {
			dispatch_semaphore_t sem = (dispatch_semaphore_t)CFBridgingRelease(m_semaphores[i]);
			(void)sem;
			m_semaphores[i] = nullptr;
		}
	}
}

void MetalFrameSync::SetMaxFramesInFlight(size_t max_in_flight) {
	m_max_in_flight = std::clamp<size_t>(max_in_flight, 1, 3);
	for (size_t i = 0; i < 3; ++i) {
		if (m_semaphores[i]) {
			dispatch_semaphore_t sem = (dispatch_semaphore_t)CFBridgingRelease(m_semaphores[i]);
			(void)sem;
			m_semaphores[i] = nullptr;
		}
	}
	for (size_t i = 0; i < m_max_in_flight; ++i) {
		dispatch_semaphore_t sem = dispatch_semaphore_create(1);
		m_semaphores[i]           = (void*)CFBridgingRetain((id)sem);
	}
}

void MetalFrameSync::BeginFrame() {
	size_t idx = m_frame_index.load() % m_max_in_flight;
	if (m_semaphores[idx]) {
		dispatch_semaphore_t sem = (__bridge dispatch_semaphore_t)m_semaphores[idx];
		dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	}
}

void MetalFrameSync::EndFrame(void* command_buffer_handle) {
	size_t idx = m_frame_index.load() % m_max_in_flight;
	dispatch_semaphore_t sem = m_semaphores[idx] ? (__bridge dispatch_semaphore_t)m_semaphores[idx] : nullptr;

	if (command_buffer_handle && sem) {
		id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)command_buffer_handle;
		[cb addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
			dispatch_semaphore_signal(sem);
		}];
	} else if (sem) {
		dispatch_semaphore_signal(sem);
	}

	m_frame_index.fetch_add(1, std::memory_order_relaxed);
	m_total_frames.fetch_add(1, std::memory_order_relaxed);
}

} // namespace Libs::Graphics::HostGpu::Metal
