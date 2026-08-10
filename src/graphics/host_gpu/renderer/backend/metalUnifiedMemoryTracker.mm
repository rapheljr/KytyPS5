// metalUnifiedMemoryTracker.mm
//
// Unified Memory Virtual Page Protection & Dirty Range Tracking Implementation.

#include "graphics/host_gpu/renderer/backend/metalUnifiedMemoryTracker.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Graphics::HostGpu {

struct TrackedBufferEntry {
	uint32_t handle;
	uint64_t gpu_addr;
	void*    host_ptr;
	size_t   size_bytes;
	std::vector<uint8_t> dirty_pages; // 4KB granularity
};

struct MetalUnifiedMemoryTracker::Impl {
	id<MTLDevice> device = nil;
	std::mutex    mutex;
	uint32_t      next_handle = 1;
	std::unordered_map<uint32_t, TrackedBufferEntry> buffers;

	~Impl() {
		buffers.clear();
		device = nil;
	}
};

MetalUnifiedMemoryTracker::MetalUnifiedMemoryTracker() : m_impl(std::make_unique<Impl>()) {}
MetalUnifiedMemoryTracker::~MetalUnifiedMemoryTracker() { Shutdown(); }

bool MetalUnifiedMemoryTracker::Initialize() {
	if (m_initialized) return true;

	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			std::cerr << "[MetalUnifiedMemoryTracker] No default Metal device available\n";
			return false;
		}

		m_stats = {};
		m_initialized = true;
		return true;
	}
}

void MetalUnifiedMemoryTracker::Shutdown() {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	m_impl->buffers.clear();
	m_impl->device = nil;
	m_initialized = false;
}

uint32_t MetalUnifiedMemoryTracker::RegisterBuffer(uint64_t gpu_addr, void* host_ptr, size_t size_bytes) {
	if (!m_initialized || !host_ptr || size_bytes == 0) return 0;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	uint32_t handle = m_impl->next_handle++;

	TrackedBufferEntry entry;
	entry.handle     = handle;
	entry.gpu_addr   = gpu_addr;
	entry.host_ptr   = host_ptr;
	entry.size_bytes = size_bytes;

	size_t page_count = (size_bytes + 4095) / 4096;
	entry.dirty_pages.resize(page_count, 0);

	m_impl->buffers[handle] = std::move(entry);
	m_stats.total_tracked_bytes += size_bytes;

	return handle;
}

void MetalUnifiedMemoryTracker::MarkRangeDirty(uint32_t handle, size_t offset, size_t size) {
	if (!m_initialized || size == 0) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	auto it = m_impl->buffers.find(handle);
	if (it == m_impl->buffers.end()) return;

	size_t start_page = offset / 4096;
	size_t end_page   = (offset + size + 4095) / 4096;
	if (end_page > it->second.dirty_pages.size()) {
		end_page = it->second.dirty_pages.size();
	}

	for (size_t p = start_page; p < end_page; ++p) {
		it->second.dirty_pages[p] = 1;
	}
}

void MetalUnifiedMemoryTracker::SynchronizeDirtyRanges(uint32_t handle) {
	if (!m_initialized) return;

	std::lock_guard<std::mutex> lock(m_impl->mutex);
	auto it = m_impl->buffers.find(handle);
	if (it == m_impl->buffers.end()) return;

	uint64_t flushed = 0;
	for (size_t p = 0; p < it->second.dirty_pages.size(); ++p) {
		if (it->second.dirty_pages[p]) {
			flushed++;
			it->second.dirty_pages[p] = 0;
		}
	}

	m_stats.total_dirty_pages_flushed += flushed;
	m_stats.sync_barrier_count++;
}

} // namespace Graphics::HostGpu
