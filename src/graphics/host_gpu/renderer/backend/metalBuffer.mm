#include "graphics/host_gpu/renderer/backend/metalBuffer.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <cstring>
#include <algorithm>

namespace Libs::Graphics {

static MTLResourceOptions ToMTLResourceOptions(MetalBufferMemoryType mem_type) {
	switch (mem_type) {
		case MetalBufferMemoryType::Shared:
			return MTLResourceStorageModeShared;
		case MetalBufferMemoryType::Private:
			return MTLResourceStorageModePrivate;
		case MetalBufferMemoryType::Managed:
#if defined(TARGET_OS_OSX) && !defined(TARGET_OS_IPHONE)
			return MTLResourceStorageModeManaged;
#else
			return MTLResourceStorageModeShared;
#endif
		case MetalBufferMemoryType::Staging:
			return MTLResourceStorageModeShared;
	}
	return MTLResourceStorageModeShared;
}

MetalBuffer::~MetalBuffer() {
	Destroy();
}

MetalBuffer::MetalBuffer(MetalBuffer&& other) noexcept
	: m_buffer(other.m_buffer)
	, m_size(other.m_size)
	, m_usage(other.m_usage)
	, m_mem_type(other.m_mem_type)
	, m_mapped_ptr(other.m_mapped_ptr) {
	other.m_buffer = nullptr;
	other.m_size = 0;
	other.m_mapped_ptr = nullptr;
}

MetalBuffer& MetalBuffer::operator=(MetalBuffer&& other) noexcept {
	if (this != &other) {
		Destroy();
		m_buffer = other.m_buffer;
		m_size = other.m_size;
		m_usage = other.m_usage;
		m_mem_type = other.m_mem_type;
		m_mapped_ptr = other.m_mapped_ptr;

		other.m_buffer = nullptr;
		other.m_size = 0;
		other.m_mapped_ptr = nullptr;
	}
	return *this;
}

bool MetalBuffer::Create(void* mtl_device, uint64_t size_bytes, MetalBufferUsage usage, MetalBufferMemoryType mem_type, const void* initial_data) {
	if (mtl_device == nullptr || size_bytes == 0) {
		return false;
	}
	if (m_buffer != nullptr) {
		Destroy();
	}

	id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;
	MTLResourceOptions options = ToMTLResourceOptions(mem_type);

	id<MTLBuffer> buffer = nil;
	if (initial_data != nullptr && mem_type != MetalBufferMemoryType::Private) {
		buffer = [device newBufferWithBytes:initial_data length:static_cast<NSUInteger>(size_bytes) options:options];
	} else {
		buffer = [device newBufferWithLength:static_cast<NSUInteger>(size_bytes) options:options];
	}

	if (buffer == nil) {
		return false;
	}

	m_buffer   = (void*)CFBridgingRetain(buffer);
	m_size     = size_bytes;
	m_usage    = usage;
	m_mem_type = mem_type;

	if (mem_type != MetalBufferMemoryType::Private) {
		m_mapped_ptr = [buffer contents];
	} else {
		m_mapped_ptr = nullptr;
	}

	if (initial_data != nullptr && mem_type == MetalBufferMemoryType::Private) {
		// Private storage mode initial data write using staging temporary buffer
		id<MTLBuffer> staging = [device newBufferWithBytes:initial_data length:static_cast<NSUInteger>(size_bytes) options:MTLResourceStorageModeShared];
		id<MTLCommandQueue> queue = [device newCommandQueue];
		id<MTLCommandBuffer> cmd_buf = [queue commandBuffer];
		id<MTLBlitCommandEncoder> blit = [cmd_buf blitCommandEncoder];
		[blit copyFromBuffer:staging sourceOffset:0 toBuffer:buffer destinationOffset:0 size:static_cast<NSUInteger>(size_bytes)];
		[blit endEncoding];
		[cmd_buf commit];
		[cmd_buf waitUntilCompleted];
	}

	return true;
}

void MetalBuffer::Destroy() {
	if (m_buffer != nullptr) {
		CFBridgingRelease(m_buffer);
		m_buffer = nullptr;
	}
	m_size       = 0;
	m_mapped_ptr = nullptr;
}

void* MetalBuffer::Map() {
	if (m_buffer == nullptr || m_mem_type == MetalBufferMemoryType::Private) {
		return nullptr;
	}
	id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)m_buffer;
	m_mapped_ptr = [buffer contents];
	return m_mapped_ptr;
}

void MetalBuffer::Unmap() {
	// For Shared memory mode on Apple Silicon, unmapping is a no-op as contents remains valid
}

bool MetalBuffer::Write(const void* src, uint64_t size, uint64_t offset) {
	if (src == nullptr || m_buffer == nullptr || (offset + size) > m_size) {
		return false;
	}
	if (m_mem_type == MetalBufferMemoryType::Private) {
		return false; // Direct CPU writes not permitted on Private VRAM
	}
	void* dst = Map();
	if (dst == nullptr) {
		return false;
	}
	std::memcpy(static_cast<uint8_t*>(dst) + offset, src, static_cast<size_t>(size));
	Flush(offset, size);
	return true;
}

bool MetalBuffer::Read(void* dst, uint64_t size, uint64_t offset) {
	if (dst == nullptr || m_buffer == nullptr || (offset + size) > m_size) {
		return false;
	}
	if (m_mem_type == MetalBufferMemoryType::Private) {
		return false; // Direct CPU reads not permitted on Private VRAM
	}
	void* src = Map();
	if (src == nullptr) {
		return false;
	}
	std::memcpy(dst, static_cast<const uint8_t*>(src) + offset, static_cast<size_t>(size));
	return true;
}

void MetalBuffer::Flush(uint64_t offset, uint64_t size) {
#if defined(TARGET_OS_OSX) && !defined(TARGET_OS_IPHONE)
	if (m_buffer != nullptr && m_mem_type == MetalBufferMemoryType::Managed) {
		id<MTLBuffer> buffer = (__bridge id<MTLBuffer>)m_buffer;
		uint64_t flush_size = (size == 0) ? (m_size - offset) : size;
		[buffer didModifyRange:NSMakeRange(static_cast<NSUInteger>(offset), static_cast<NSUInteger>(flush_size))];
	}
#else
	(void)offset; (void)size;
#endif
}

MetalRingBuffer::~MetalRingBuffer() {
	Shutdown();
}

bool MetalRingBuffer::Initialize(void* mtl_device, uint64_t capacity_bytes, MetalBufferUsage usage) {
	if (mtl_device == nullptr || capacity_bytes == 0) {
		return false;
	}
	Shutdown();
	m_capacity = capacity_bytes;
	m_head = 0;
	m_allocated_bytes = 0;
	return m_buffer.Create(mtl_device, capacity_bytes, usage | MetalBufferUsage::Dynamic, MetalBufferMemoryType::Shared);
}

void MetalRingBuffer::Shutdown() {
	m_buffer.Destroy();
	m_capacity = 0;
	m_head = 0;
	m_allocated_bytes = 0;
}

MetalRingBuffer::Allocation MetalRingBuffer::Allocate(uint64_t size_bytes, uint64_t alignment) {
	Allocation alloc {};
	if (m_buffer.GetMTLBuffer() == nullptr || size_bytes == 0 || size_bytes > m_capacity) {
		return alloc;
	}

	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t current_head = m_head.load(std::memory_order_relaxed);
	uint64_t aligned_offset = (current_head + (alignment - 1)) & ~(alignment - 1);

	if (aligned_offset + size_bytes > m_capacity) {
		// Wrap around to start of buffer
		aligned_offset = 0;
	}

	if (aligned_offset + size_bytes > m_capacity) {
		return alloc; // Cannot fit allocation in ring buffer
	}

	m_head.store(aligned_offset + size_bytes, std::memory_order_relaxed);
	m_allocated_bytes.fetch_add(size_bytes, std::memory_order_relaxed);

	alloc.mtl_buffer   = m_buffer.GetMTLBuffer();
	alloc.mapped_ptr   = static_cast<uint8_t*>(m_buffer.GetMappedPointer()) + aligned_offset;
	alloc.offset_bytes = aligned_offset;
	alloc.size_bytes   = size_bytes;
	alloc.valid        = true;

	return alloc;
}

void MetalRingBuffer::Reset() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_head.store(0, std::memory_order_relaxed);
	m_allocated_bytes.store(0, std::memory_order_relaxed);
}

} // namespace Libs::Graphics

#endif // __APPLE__
