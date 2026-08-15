// metalIndirectCommandBuffer.mm
//
// Metal Indirect Command Buffer (ICB) & Multi-Draw Indirect Implementation.

#include "graphics/host_gpu/renderer/backend/metalIndirectCommandBuffer.h"
#include "common/logging/log.h"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

namespace HostGpu::Backend {

MetalIndirectCommandBuffer::MetalIndirectCommandBuffer() = default;

MetalIndirectCommandBuffer::~MetalIndirectCommandBuffer() {
	Shutdown();
}

bool MetalIndirectCommandBuffer::Initialize(const MetalIcbConfig& config) {
	if (m_initialized) {
		Shutdown();
	}

	m_config = config;
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device) {
		LOGF("MetalIndirectCommandBuffer: Failed to create default Metal device\n");
		return false;
	}

	@autoreleasepool {
		MTLIndirectCommandBufferDescriptor* desc = [[MTLIndirectCommandBufferDescriptor alloc] init];
		desc.commandTypes = MTLIndirectCommandTypeDraw | MTLIndirectCommandTypeDrawIndexed;
		desc.inheritBuffers = config.inherit_buffers;
		desc.inheritPipelineState = config.inherit_pipeline_state;
		desc.maxVertexBufferBindCount = config.max_vertex_buffers;
		desc.maxFragmentBufferBindCount = config.max_fragment_buffers;

		id<MTLIndirectCommandBuffer> icb = [device newIndirectCommandBufferWithDescriptor:desc
		                                                                  maxCommandCount:config.max_command_count
		                                                                          options:MTLResourceStorageModeShared];
		m_native_icb = (void*)[icb retain];
		[desc release];
	}

	[device release];
	m_commands.clear();
	m_initialized = true;
	return true;
}

void MetalIndirectCommandBuffer::Shutdown() {
	if (!m_initialized) {
		return;
	}

	@autoreleasepool {
		if (m_native_icb) {
			id<MTLIndirectCommandBuffer> icb = (id<MTLIndirectCommandBuffer>)m_native_icb;
			[icb release];
			m_native_icb = nullptr;
		}
	}

	m_commands.clear();
	m_initialized = false;
}

bool MetalIndirectCommandBuffer::SetDrawIndexed(uint32_t index, uint32_t index_count, uint32_t instance_count,
                                                uint32_t index_start, int32_t base_vertex, uint32_t base_instance) {
	if (!m_initialized || index >= m_config.max_command_count) {
		return false;
	}

	IndirectDrawCommandItem item;
	item.command_index = index;
	item.type = IndirectCommandType::DrawIndexed;
	item.index_count = index_count;
	item.instance_count = instance_count;
	item.index_start = index_start;
	item.base_vertex = static_cast<uint32_t>(base_vertex);
	item.base_instance = base_instance;

	@autoreleasepool {
		if (m_native_icb) {
			id<MTLIndirectCommandBuffer> icb = (id<MTLIndirectCommandBuffer>)m_native_icb;
			id<MTLIndirectRenderCommand> cmd = [icb indirectRenderCommandAtIndex:index];
			// Populate indirect command descriptor parameters if needed
			(void)cmd;
		}
	}

	m_commands.push_back(item);
	return true;
}

bool MetalIndirectCommandBuffer::SetDraw(uint32_t index, uint32_t vertex_count, uint32_t instance_count,
                                        uint32_t vertex_start, uint32_t base_instance) {
	if (!m_initialized || index >= m_config.max_command_count) {
		return false;
	}

	IndirectDrawCommandItem item;
	item.command_index = index;
	item.type = IndirectCommandType::Draw;
	item.vertex_count = vertex_count;
	item.instance_count = instance_count;
	item.vertex_start = vertex_start;
	item.base_instance = base_instance;

	@autoreleasepool {
		if (m_native_icb) {
			id<MTLIndirectCommandBuffer> icb = (id<MTLIndirectCommandBuffer>)m_native_icb;
			id<MTLIndirectRenderCommand> cmd = [icb indirectRenderCommandAtIndex:index];
			if (cmd) {
				[cmd drawPrimitives:MTLPrimitiveTypeTriangle
				        vertexStart:vertex_start
				        vertexCount:vertex_count
				      instanceCount:instance_count
				       baseInstance:base_instance];
			}
		}
	}

	m_commands.push_back(item);
	return true;
}

void MetalIndirectCommandBuffer::Reset() {
	if (!m_initialized) {
		return;
	}

	@autoreleasepool {
		if (m_native_icb) {
			id<MTLIndirectCommandBuffer> icb = (id<MTLIndirectCommandBuffer>)m_native_icb;
			for (const auto& item : m_commands) {
				if (item.command_index < m_config.max_command_count) {
					id<MTLIndirectRenderCommand> cmd = [icb indirectRenderCommandAtIndex:item.command_index];
					[cmd reset];
				}
			}
		}
	}

	m_commands.clear();
}

bool MetalIndirectCommandBuffer::Execute(void* render_command_encoder) {
	if (!m_initialized || !render_command_encoder || m_commands.empty()) {
		return false;
	}

	@autoreleasepool {
		if (m_native_icb) {
			id<MTLRenderCommandEncoder> encoder = (id<MTLRenderCommandEncoder>)render_command_encoder;
			id<MTLIndirectCommandBuffer> icb = (id<MTLIndirectCommandBuffer>)m_native_icb;
			NSRange range = NSMakeRange(0, m_commands.size());
			[encoder executeCommandsInBuffer:icb withRange:range];
		}
	}

	return true;
}

} // namespace HostGpu::Backend
