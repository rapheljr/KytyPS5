#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"
#include "common/timer.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#endif

namespace Libs::Graphics {

MetalCommandBuffer::MetalCommandBuffer(void* mtl_command_queue) {
#if defined(__APPLE__)
	if (mtl_command_queue == nullptr) {
		m_state = MetalCommandBufferState::Error;
		return;
	}
	id<MTLCommandQueue> queue = (__bridge id<MTLCommandQueue>)mtl_command_queue;
	id<MTLCommandBuffer> cb   = [queue commandBuffer];
	if (cb == nil) {
		m_state = MetalCommandBufferState::Error;
		return;
	}
	m_command_buffer = (void*)CFBridgingRetain(cb);
	m_state          = MetalCommandBufferState::Recording;
#else
	(void)mtl_command_queue;
	m_state = MetalCommandBufferState::Error;
#endif
}

MetalCommandBuffer::~MetalCommandBuffer() {
#if defined(__APPLE__)
	// Close any dangling encoder
	if (m_compute_encoder != nullptr) {
		CloseComputeEncoder();
	}
	// If committed but not waited, synchronize now to prevent GPU faults
	if (m_state == MetalCommandBufferState::Committed && !m_waited) {
		WaitUntilCompleted();
	}
	if (m_command_buffer != nullptr) {
		CFBridgingRelease(m_command_buffer);
		m_command_buffer = nullptr;
	}
#endif
}

bool MetalCommandBuffer::IsValid() const noexcept {
	return m_command_buffer != nullptr && m_state != MetalCommandBufferState::Error &&
	       m_state != MetalCommandBufferState::NotAllocated;
}

MetalCommandBufferState MetalCommandBuffer::GetState() const noexcept {
	return m_state;
}

void* MetalCommandBuffer::OpenComputeEncoder() {
#if defined(__APPLE__)
	if (m_state != MetalCommandBufferState::Recording || m_compute_encoder != nullptr) {
		return nullptr;
	}
	id<MTLCommandBuffer>         cb  = (__bridge id<MTLCommandBuffer>)m_command_buffer;
	id<MTLComputeCommandEncoder> enc = [cb computeCommandEncoder];
	if (enc == nil) {
		return nullptr;
	}
	m_compute_encoder = (void*)CFBridgingRetain(enc);
	return m_compute_encoder;
#else
	return nullptr;
#endif
}

void MetalCommandBuffer::CloseComputeEncoder() {
#if defined(__APPLE__)
	if (m_compute_encoder == nullptr) {
		return;
	}
	id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)m_compute_encoder;
	[enc endEncoding];
	CFBridgingRelease(m_compute_encoder);
	m_compute_encoder = nullptr;
#endif
}

void MetalCommandBuffer::DispatchThreadgroups(uint32_t groups_x, uint32_t groups_y, uint32_t groups_z,
                                              uint32_t threads_per_group_x, uint32_t threads_per_group_y, uint32_t threads_per_group_z) {
#if defined(__APPLE__)
	if (m_compute_encoder == nullptr) {
		return;
	}
	id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)m_compute_encoder;
	MTLSize threadgroupsPerGrid = MTLSizeMake(groups_x > 0 ? groups_x : 1,
	                                          groups_y > 0 ? groups_y : 1,
	                                          groups_z > 0 ? groups_z : 1);
	MTLSize threadsPerGroup     = MTLSizeMake(threads_per_group_x > 0 ? threads_per_group_x : 1,
	                                          threads_per_group_y > 0 ? threads_per_group_y : 1,
	                                          threads_per_group_z > 0 ? threads_per_group_z : 1);
	[enc dispatchThreadgroups:threadgroupsPerGrid threadsPerThreadgroup:threadsPerGroup];
#else
	(void)groups_x; (void)groups_y; (void)groups_z;
	(void)threads_per_group_x; (void)threads_per_group_y; (void)threads_per_group_z;
#endif
}

void MetalCommandBuffer::DispatchIndirect(void* indirect_buffer, size_t indirect_offset,
                                          uint32_t threads_per_group_x, uint32_t threads_per_group_y, uint32_t threads_per_group_z) {
#if defined(__APPLE__)
	if (m_compute_encoder == nullptr || indirect_buffer == nullptr) {
		return;
	}
	id<MTLComputeCommandEncoder> enc = (__bridge id<MTLComputeCommandEncoder>)m_compute_encoder;
	id<MTLBuffer> buf                = (__bridge id<MTLBuffer>)indirect_buffer;
	MTLSize threadsPerGroup          = MTLSizeMake(threads_per_group_x > 0 ? threads_per_group_x : 1,
	                                               threads_per_group_y > 0 ? threads_per_group_y : 1,
	                                               threads_per_group_z > 0 ? threads_per_group_z : 1);
	[enc dispatchThreadgroupsWithIndirectBuffer:buf
	                       indirectBufferOffset:static_cast<NSUInteger>(indirect_offset)
	                      threadsPerThreadgroup:threadsPerGroup];
#else
	(void)indirect_buffer; (void)indirect_offset;
	(void)threads_per_group_x; (void)threads_per_group_y; (void)threads_per_group_z;
#endif
}

void MetalCommandBuffer::Commit() {
#if defined(__APPLE__)
	if (m_state != MetalCommandBufferState::Recording) {
		return;
	}
	// Close any open encoder before committing
	if (m_compute_encoder != nullptr) {
		CloseComputeEncoder();
	}
	id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)m_command_buffer;
	[cb commit];
	m_state   = MetalCommandBufferState::Committed;
	m_waited  = false;
#endif
}

void MetalCommandBuffer::WaitUntilCompleted() {
#if defined(__APPLE__)
	if (m_state != MetalCommandBufferState::Committed) {
		return;
	}
	const uint64_t t0            = Common::Timer::QueryPerformanceCounter();
	id<MTLCommandBuffer> cb      = (__bridge id<MTLCommandBuffer>)m_command_buffer;
	[cb waitUntilCompleted];
	const uint64_t t1            = Common::Timer::QueryPerformanceCounter();
	const uint64_t freq          = Common::Timer::QueryPerformanceFrequency();
	m_gpu_time_ns = (freq > 0) ? ((t1 - t0) * 1000000000ULL / freq) : 0;
	m_waited      = true;
	m_state       = MetalCommandBufferState::Completed;
#endif
}

uint64_t MetalCommandBuffer::GetGpuExecutionTimeNs() const noexcept {
	return m_gpu_time_ns;
}

} // namespace Libs::Graphics
