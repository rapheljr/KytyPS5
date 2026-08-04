#include "graphics/host_gpu/renderer/backend/metalBuffer.h"

namespace Libs::Graphics {

#if !defined(__APPLE__)

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

bool MetalBuffer::Create(void* /*mtl_device*/, uint64_t /*size_bytes*/, MetalBufferUsage /*usage*/, MetalBufferMemoryType /*mem_type*/, const void* /*initial_data*/) {
	return false;
}

void MetalBuffer::Destroy() {
	m_buffer = nullptr;
	m_size = 0;
	m_mapped_ptr = nullptr;
}

void* MetalBuffer::Map() {
	return nullptr;
}

void MetalBuffer::Unmap() {
}

bool MetalBuffer::Write(const void* /*src*/, uint64_t /*size*/, uint64_t /*offset*/) {
	return false;
}

bool MetalBuffer::Read(void* /*dst*/, uint64_t /*size*/, uint64_t /*offset*/) {
	return false;
}

void MetalBuffer::Flush(uint64_t /*offset*/, uint64_t /*size*/) {
}

MetalRingBuffer::~MetalRingBuffer() {
	Shutdown();
}

bool MetalRingBuffer::Initialize(void* /*mtl_device*/, uint64_t /*capacity_bytes*/, MetalBufferUsage /*usage*/) {
	return false;
}

void MetalRingBuffer::Shutdown() {
	m_capacity = 0;
	m_head = 0;
	m_allocated_bytes = 0;
}

MetalRingBuffer::Allocation MetalRingBuffer::Allocate(uint64_t /*size_bytes*/, uint64_t /*alignment*/) {
	return {};
}

void MetalRingBuffer::Reset() {
	m_head = 0;
	m_allocated_bytes = 0;
}

#endif // !defined(__APPLE__)

} // namespace Libs::Graphics
