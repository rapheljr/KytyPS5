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
	if (m_render_encoder != nullptr) {
		CloseRenderEncoder();
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
	if (m_state != MetalCommandBufferState::Recording || m_compute_encoder != nullptr || m_render_encoder != nullptr) {
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

void* MetalCommandBuffer::OpenRenderEncoder(void* mtl_render_pass_descriptor) {
#if defined(__APPLE__)
	if (m_state != MetalCommandBufferState::Recording || m_render_encoder != nullptr || m_compute_encoder != nullptr || m_command_buffer == nullptr) {
		return nullptr;
	}
	id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)m_command_buffer;
	MTLRenderPassDescriptor* desc = (__bridge MTLRenderPassDescriptor*)mtl_render_pass_descriptor;
	if (desc == nil) {
		desc = [MTLRenderPassDescriptor renderPassDescriptor];
	}
	id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:desc];
	if (enc == nil) {
		return nullptr;
	}
	m_render_encoder = (void*)CFBridgingRetain(enc);
	return m_render_encoder;
#else
	(void)mtl_render_pass_descriptor;
	return nullptr;
#endif
}

void MetalCommandBuffer::CloseRenderEncoder() {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	[enc endEncoding];
	CFBridgingRelease(m_render_encoder);
	m_render_encoder = nullptr;
#endif
}

void MetalCommandBuffer::SetViewport(float x, float y, float width, float height, float znear, float zfar) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	MTLViewport vp;
	vp.originX = x;
	vp.originY = y;
	vp.width   = width > 0.0f ? width : 1.0f;
	vp.height  = height > 0.0f ? height : 1.0f;
	vp.znear   = znear;
	vp.zfar    = zfar;
	[enc setViewport:vp];
#else
	(void)x; (void)y; (void)width; (void)height; (void)znear; (void)zfar;
#endif
}

void MetalCommandBuffer::SetScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	MTLScissorRect sr;
	sr.x      = static_cast<NSUInteger>(x);
	sr.y      = static_cast<NSUInteger>(y);
	sr.width  = static_cast<NSUInteger>(width > 0 ? width : 1);
	sr.height = static_cast<NSUInteger>(height > 0 ? height : 1);
	[enc setScissorRect:sr];
#else
	(void)x; (void)y; (void)width; (void)height;
#endif
}

void MetalCommandBuffer::SetRenderPipelineState(void* mtl_pipeline_state) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr || mtl_pipeline_state == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	id<MTLRenderPipelineState> pso = (__bridge id<MTLRenderPipelineState>)mtl_pipeline_state;
	[enc setRenderPipelineState:pso];
#else
	(void)mtl_pipeline_state;
#endif
}

void MetalCommandBuffer::SetVertexBuffer(void* mtl_buffer, size_t offset, uint32_t index) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr || mtl_buffer == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	id<MTLBuffer> buf = (__bridge id<MTLBuffer>)mtl_buffer;
	[enc setVertexBuffer:buf offset:static_cast<NSUInteger>(offset) atIndex:static_cast<NSUInteger>(index)];
#else
	(void)mtl_buffer; (void)offset; (void)index;
#endif
}

void MetalCommandBuffer::DrawPrimitives(uint32_t primitive_type, uint32_t vertex_start, uint32_t vertex_count, uint32_t instance_count) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr || vertex_count == 0) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	MTLPrimitiveType pt = MTLPrimitiveTypeTriangle;
	switch (primitive_type) {
		case 0: pt = MTLPrimitiveTypePoint; break;
		case 1: pt = MTLPrimitiveTypeLine; break;
		case 2: pt = MTLPrimitiveTypeLineStrip; break;
		case 3: pt = MTLPrimitiveTypeTriangle; break;
		case 4: pt = MTLPrimitiveTypeTriangleStrip; break;
		default: pt = MTLPrimitiveTypeTriangle; break;
	}
	[enc drawPrimitives:pt
	        vertexStart:static_cast<NSUInteger>(vertex_start)
	        vertexCount:static_cast<NSUInteger>(vertex_count)
	      instanceCount:static_cast<NSUInteger>(instance_count > 0 ? instance_count : 1)];
#else
	(void)primitive_type; (void)vertex_start; (void)vertex_count; (void)instance_count;
#endif
}

void MetalCommandBuffer::DrawIndexedPrimitives(uint32_t primitive_type, uint32_t index_count, uint32_t index_type,
                                               void* index_buffer, size_t index_buffer_offset, uint32_t instance_count) {
#if defined(__APPLE__)
	if (m_render_encoder == nullptr || index_count == 0 || index_buffer == nullptr) {
		return;
	}
	id<MTLRenderCommandEncoder> enc = (__bridge id<MTLRenderCommandEncoder>)m_render_encoder;
	MTLPrimitiveType pt = MTLPrimitiveTypeTriangle;
	switch (primitive_type) {
		case 0: pt = MTLPrimitiveTypePoint; break;
		case 1: pt = MTLPrimitiveTypeLine; break;
		case 2: pt = MTLPrimitiveTypeLineStrip; break;
		case 3: pt = MTLPrimitiveTypeTriangle; break;
		case 4: pt = MTLPrimitiveTypeTriangleStrip; break;
		default: pt = MTLPrimitiveTypeTriangle; break;
	}
	MTLIndexType it = (index_type == 1) ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
	id<MTLBuffer> ibuf = (__bridge id<MTLBuffer>)index_buffer;
	[enc drawIndexedPrimitives:pt
	                indexCount:static_cast<NSUInteger>(index_count)
	                 indexType:it
	               indexBuffer:ibuf
	         indexBufferOffset:static_cast<NSUInteger>(index_buffer_offset)
	             instanceCount:static_cast<NSUInteger>(instance_count > 0 ? instance_count : 1)];
#else
	(void)primitive_type; (void)index_count; (void)index_type; (void)index_buffer; (void)index_buffer_offset; (void)instance_count;
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
	if (m_render_encoder != nullptr) {
		CloseRenderEncoder();
	}
	id<MTLCommandBuffer> cb = (__bridge id<MTLCommandBuffer>)m_command_buffer;
	[cb commit];
	m_state   = MetalCommandBufferState::Committed;
	m_waited  = false;
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
