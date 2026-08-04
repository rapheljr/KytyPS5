#include "graphics/host_gpu/renderer/backend/metalMemoryPool.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <cstring>

namespace Libs::Graphics {

void MetalGpuMemoryStats::RecordAllocation(uint64_t size_bytes, bool is_texture) {
	allocated_bytes.fetch_add(size_bytes, std::memory_order_relaxed);
	used_bytes.fetch_add(size_bytes, std::memory_order_relaxed);

	uint64_t current_used = used_bytes.load(std::memory_order_relaxed);
	uint64_t current_peak = peak_bytes.load(std::memory_order_relaxed);
	while (current_used > current_peak && !peak_bytes.compare_exchange_weak(current_peak, current_used, std::memory_order_relaxed)) {}

	if (is_texture) {
		texture_count.fetch_add(1, std::memory_order_relaxed);
	} else {
		buffer_count.fetch_add(1, std::memory_order_relaxed);
	}
}

void MetalGpuMemoryStats::RecordDeallocation(uint64_t size_bytes, bool is_texture) {
	if (used_bytes.load(std::memory_order_relaxed) >= size_bytes) {
		used_bytes.fetch_sub(size_bytes, std::memory_order_relaxed);
	}
	if (is_texture) {
		if (texture_count.load(std::memory_order_relaxed) > 0) {
			texture_count.fetch_sub(1, std::memory_order_relaxed);
		}
	} else {
		if (buffer_count.load(std::memory_order_relaxed) > 0) {
			buffer_count.fetch_sub(1, std::memory_order_relaxed);
		}
	}
}

void MetalGpuMemoryStats::Reset() {
	allocated_bytes = 0;
	used_bytes = 0;
	peak_bytes = 0;
	buffer_count = 0;
	texture_count = 0;
	heap_count = 0;
}

MetalGpuHeapAllocator::~MetalGpuHeapAllocator() {

	Shutdown();
}

bool MetalGpuHeapAllocator::Initialize(void* mtl_device, uint64_t initial_heap_size_bytes) {
	if (mtl_device == nullptr) {
		return false;
	}
	Shutdown();
	m_mtl_device = mtl_device;

	id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;

	MTLHeapDescriptor* heap_desc = [[MTLHeapDescriptor alloc] init];
	heap_desc.size               = static_cast<NSUInteger>(initial_heap_size_bytes);
	heap_desc.storageMode        = MTLStorageModePrivate;
	heap_desc.cpuCacheMode       = MTLCPUCacheModeDefaultCache;
	heap_desc.type               = MTLHeapTypeAutomatic;

	id<MTLHeap> heap = [device newHeapWithDescriptor:heap_desc];
	if (heap == nil) {
		return false;
	}

	m_heaps.push_back((void*)CFBridgingRetain(heap));
	m_stats.heap_count.fetch_add(1, std::memory_order_relaxed);
	return true;
}

void MetalGpuHeapAllocator::Shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (void* ptr : m_heaps) {
		if (ptr != nullptr) {
			CFBridgingRelease(ptr);
		}
	}
	m_heaps.clear();
	m_mtl_device = nullptr;
	m_stats.Reset();
}

void* MetalGpuHeapAllocator::AllocateBuffer(uint64_t size_bytes, MetalBufferMemoryType /*mem_type*/) {
	if (m_mtl_device == nullptr || size_bytes == 0) {
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	id<MTLBuffer> buffer = nil;

	for (void* heap_ptr : m_heaps) {
		id<MTLHeap> heap = (__bridge id<MTLHeap>)heap_ptr;
		buffer = [heap newBufferWithLength:static_cast<NSUInteger>(size_bytes) options:MTLResourceStorageModePrivate];
		if (buffer != nil) {
			break;
		}
	}

	if (buffer == nil) {
		// Expand heap pool dynamically
		id<MTLDevice> device = (__bridge id<MTLDevice>)m_mtl_device;
		uint64_t new_heap_size = std::max<uint64_t>(16 * 1024 * 1024, size_bytes * 2);

		MTLHeapDescriptor* heap_desc = [[MTLHeapDescriptor alloc] init];
		heap_desc.size               = static_cast<NSUInteger>(new_heap_size);
		heap_desc.storageMode        = MTLStorageModePrivate;
		heap_desc.cpuCacheMode       = MTLCPUCacheModeDefaultCache;
		heap_desc.type               = MTLHeapTypeAutomatic;

		id<MTLHeap> new_heap = [device newHeapWithDescriptor:heap_desc];
		if (new_heap != nil) {
			m_heaps.push_back((void*)CFBridgingRetain(new_heap));
			m_stats.heap_count.fetch_add(1, std::memory_order_relaxed);
			buffer = [new_heap newBufferWithLength:static_cast<NSUInteger>(size_bytes) options:MTLResourceStorageModePrivate];
		}
	}

	if (buffer == nil) {
		return nullptr;
	}

	m_stats.RecordAllocation(size_bytes, false);
	return (void*)CFBridgingRetain(buffer);
}

void* MetalGpuHeapAllocator::AllocateTexture(void* mtl_texture_desc) {
	if (m_mtl_device == nullptr || mtl_texture_desc == nullptr) {
		return nullptr;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	MTLTextureDescriptor* desc = (__bridge MTLTextureDescriptor*)mtl_texture_desc;
	id<MTLTexture> texture = nil;

	for (void* heap_ptr : m_heaps) {
		id<MTLHeap> heap = (__bridge id<MTLHeap>)heap_ptr;
		texture = [heap newTextureWithDescriptor:desc];
		if (texture != nil) {
			break;
		}
	}

	if (texture == nil) {
		id<MTLDevice> device = (__bridge id<MTLDevice>)m_mtl_device;
		MTLHeapDescriptor* heap_desc = [[MTLHeapDescriptor alloc] init];
		heap_desc.size               = 32 * 1024 * 1024;
		heap_desc.storageMode        = MTLStorageModePrivate;
		heap_desc.type               = MTLHeapTypeAutomatic;

		id<MTLHeap> new_heap = [device newHeapWithDescriptor:heap_desc];
		if (new_heap != nil) {
			m_heaps.push_back((void*)CFBridgingRetain(new_heap));
			m_stats.heap_count.fetch_add(1, std::memory_order_relaxed);
			texture = [new_heap newTextureWithDescriptor:desc];
		}
	}

	if (texture == nil) {
		return nullptr;
	}

	m_stats.RecordAllocation([texture allocatedSize], true);
	return (void*)CFBridgingRetain(texture);
}

uint64_t MetalGpuHeapAllocator::GetTotalHeapBytes() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	uint64_t total = 0;
	for (void* heap_ptr : m_heaps) {
		id<MTLHeap> heap = (__bridge id<MTLHeap>)heap_ptr;
		total += [heap size];
	}
	return total;
}

size_t MetalGpuHeapAllocator::GetHeapCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_heaps.size();
}

MetalResourceDeferrer::~MetalResourceDeferrer() {
	Shutdown();
}

void MetalResourceDeferrer::Initialize() {
	Shutdown();
}

void MetalResourceDeferrer::Shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (const auto& item : m_pending_queue) {
		if (item.resource != nullptr) {
			CFBridgingRelease(item.resource);
		}
	}
	m_pending_queue.clear();
}

void MetalResourceDeferrer::DeferRelease(void* mtl_resource, uint64_t frame_index) {
	if (mtl_resource == nullptr) {
		return;
	}
	std::lock_guard<std::mutex> lock(m_mutex);
	m_pending_queue.push_back({mtl_resource, frame_index});
}

void MetalResourceDeferrer::ProcessDeferredReleases(uint64_t completed_frame_index) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_pending_queue.begin();
	while (it != m_pending_queue.end()) {
		if (it->frame_index <= completed_frame_index) {
			if (it->resource != nullptr) {
				CFBridgingRelease(it->resource);
			}
			it = m_pending_queue.erase(it);
		} else {
			++it;
		}
	}
}

size_t MetalResourceDeferrer::GetPendingReleaseCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_pending_queue.size();
}

MetalUploadStaging::~MetalUploadStaging() {
	Shutdown();
}

bool MetalUploadStaging::Initialize(void* mtl_device, uint64_t default_capacity_bytes) {
	if (mtl_device == nullptr || default_capacity_bytes == 0) {
		return false;
	}
	Shutdown();
	m_mtl_device = mtl_device;
	m_head = 0;
	return m_staging_buffer.Create(mtl_device, default_capacity_bytes, MetalBufferUsage::Upload, MetalBufferMemoryType::Shared);
}

void MetalUploadStaging::Shutdown() {
	m_staging_buffer.Destroy();
	m_mtl_device = nullptr;
	m_head = 0;
}

MetalUploadStaging::StagingAllocation MetalUploadStaging::StageUpload(const void* src_bytes, uint64_t size_bytes, uint64_t alignment) {
	StagingAllocation alloc {};
	if (src_bytes == nullptr || size_bytes == 0 || m_staging_buffer.GetMTLBuffer() == nullptr) {
		return alloc;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	uint64_t cur_head = m_head.load(std::memory_order_relaxed);
	uint64_t aligned_off = (cur_head + (alignment - 1)) & ~(alignment - 1);

	if (aligned_off + size_bytes > m_staging_buffer.GetSize()) {
		aligned_off = 0; // Wrap around staging buffer
	}

	if (aligned_off + size_bytes > m_staging_buffer.GetSize()) {
		return alloc;
	}

	m_head.store(aligned_off + size_bytes, std::memory_order_relaxed);
	void* mapped = static_cast<uint8_t*>(m_staging_buffer.GetMappedPointer()) + aligned_off;
	std::memcpy(mapped, src_bytes, static_cast<size_t>(size_bytes));

	alloc.buffer     = &m_staging_buffer;
	alloc.mapped_ptr = mapped;
	alloc.offset     = aligned_off;
	alloc.size       = size_bytes;
	alloc.valid      = true;

	return alloc;
}

void MetalUploadStaging::ResetPool() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_head.store(0, std::memory_order_relaxed);
}

MetalReadbackStaging::~MetalReadbackStaging() {
	Shutdown();
}

bool MetalReadbackStaging::Initialize(void* mtl_device, uint64_t capacity_bytes) {
	if (mtl_device == nullptr || capacity_bytes == 0) {
		return false;
	}
	Shutdown();
	m_mtl_device = mtl_device;
	m_head = 0;
	return m_readback_buffer.Create(mtl_device, capacity_bytes, MetalBufferUsage::Readback, MetalBufferMemoryType::Shared);
}

void MetalReadbackStaging::Shutdown() {
	m_readback_buffer.Destroy();
	m_mtl_device = nullptr;
	m_head = 0;
}

MetalReadbackStaging::ReadbackAllocation MetalReadbackStaging::AllocateReadback(uint64_t size_bytes, uint64_t alignment) {
	ReadbackAllocation alloc {};
	if (size_bytes == 0 || m_readback_buffer.GetMTLBuffer() == nullptr) {
		return alloc;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	uint64_t cur_head = m_head.load(std::memory_order_relaxed);
	uint64_t aligned_off = (cur_head + (alignment - 1)) & ~(alignment - 1);

	if (aligned_off + size_bytes > m_readback_buffer.GetSize()) {
		aligned_off = 0;
	}

	if (aligned_off + size_bytes > m_readback_buffer.GetSize()) {
		return alloc;
	}

	m_head.store(aligned_off + size_bytes, std::memory_order_relaxed);

	alloc.buffer     = &m_readback_buffer;
	alloc.mapped_ptr = static_cast<uint8_t*>(m_readback_buffer.GetMappedPointer()) + aligned_off;
	alloc.offset     = aligned_off;
	alloc.size       = size_bytes;
	alloc.valid      = true;

	return alloc;
}

void MetalReadbackStaging::ResetPool() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_head.store(0, std::memory_order_relaxed);
}

} // namespace Libs::Graphics

#endif // __APPLE__
