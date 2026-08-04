// metalPipelineCache_stub.cpp
// Non-Apple stub — all methods operate safely in fallback mode when Metal is unavailable.

#include "graphics/host_gpu/renderer/backend/metalPipelineCache.h"

#if !defined(__APPLE__)

namespace Libs::Graphics {

MetalPipelineCache::MetalPipelineCache(void* mtl_device,
                                       size_t max_graphics_capacity,
                                       size_t max_compute_capacity)
    : m_device(mtl_device),
      m_max_graphics_capacity(max_graphics_capacity),
      m_max_compute_capacity(max_compute_capacity) {}

MetalPipelineCache::~MetalPipelineCache() {
	Clear();
}

MetalRenderPipelineEntry* MetalPipelineCache::GetOrCreateGraphicsPipeline(const MetalGraphicsPipelineKey& key) {
	Common::LockGuard lock(m_mutex);

	auto it = m_graphics_pipelines.find(key);
	if (it != m_graphics_pipelines.end()) {
		++m_graphics_hits;
		it->second->last_used_timestamp = ++m_timestamp_counter;
		++it->second->use_count;
		return it->second.get();
	}

	++m_graphics_misses;

	if (m_graphics_pipelines.size() >= m_max_graphics_capacity && m_max_graphics_capacity > 0) {
		EvictLRUGraphics(m_max_graphics_capacity - 1);
	}

	auto entry = std::unique_ptr<MetalRenderPipelineEntry>(CompileGraphicsPipeline(key));
	if (entry == nullptr) {
		return nullptr;
	}

	entry->last_used_timestamp = ++m_timestamp_counter;
	entry->use_count = 1;
	m_total_memory_bytes += entry->estimated_bytes;

	MetalRenderPipelineEntry* ptr = entry.get();
	m_graphics_pipelines.emplace(key, std::move(entry));
	return ptr;
}

MetalComputePipelineEntry* MetalPipelineCache::GetOrCreateComputePipeline(const MetalComputePipelineKey& key) {
	Common::LockGuard lock(m_mutex);

	auto it = m_compute_pipelines.find(key);
	if (it != m_compute_pipelines.end()) {
		++m_compute_hits;
		it->second->last_used_timestamp = ++m_timestamp_counter;
		++it->second->use_count;
		return it->second.get();
	}

	++m_compute_misses;

	if (m_compute_pipelines.size() >= m_max_compute_capacity && m_max_compute_capacity > 0) {
		EvictLRUCompute(m_max_compute_capacity - 1);
	}

	auto entry = std::unique_ptr<MetalComputePipelineEntry>(CompileComputePipeline(key));
	if (entry == nullptr) {
		return nullptr;
	}

	entry->last_used_timestamp = ++m_timestamp_counter;
	entry->use_count = 1;
	m_total_memory_bytes += entry->estimated_bytes;

	MetalComputePipelineEntry* ptr = entry.get();
	m_compute_pipelines.emplace(key, std::move(entry));
	return ptr;
}

MetalRenderPipelineEntry* MetalPipelineCache::CompileGraphicsPipeline(const MetalGraphicsPipelineKey& /*key*/) {
	auto* entry = new MetalRenderPipelineEntry();
	entry->estimated_bytes = 4096;
	return entry;
}

MetalComputePipelineEntry* MetalPipelineCache::CompileComputePipeline(const MetalComputePipelineKey& /*key*/) {
	auto* entry = new MetalComputePipelineEntry();
	entry->estimated_bytes = 2048;
	return entry;
}

void MetalPipelineCache::EvictLRUGraphics(size_t target_capacity) {
	while (m_graphics_pipelines.size() > target_capacity && !m_graphics_pipelines.empty()) {
		auto lru_it = m_graphics_pipelines.begin();
		uint64_t oldest_ts = lru_it->second->last_used_timestamp;

		for (auto it = m_graphics_pipelines.begin(); it != m_graphics_pipelines.end(); ++it) {
			if (it->second->last_used_timestamp < oldest_ts) {
				oldest_ts = it->second->last_used_timestamp;
				lru_it = it;
			}
		}

		if (m_total_memory_bytes >= lru_it->second->estimated_bytes) {
			m_total_memory_bytes -= lru_it->second->estimated_bytes;
		}
		m_graphics_pipelines.erase(lru_it);
	}
}

void MetalPipelineCache::EvictLRUCompute(size_t target_capacity) {
	while (m_compute_pipelines.size() > target_capacity && !m_compute_pipelines.empty()) {
		auto lru_it = m_compute_pipelines.begin();
		uint64_t oldest_ts = lru_it->second->last_used_timestamp;

		for (auto it = m_compute_pipelines.begin(); it != m_compute_pipelines.end(); ++it) {
			if (it->second->last_used_timestamp < oldest_ts) {
				oldest_ts = it->second->last_used_timestamp;
				lru_it = it;
			}
		}

		if (m_total_memory_bytes >= lru_it->second->estimated_bytes) {
			m_total_memory_bytes -= lru_it->second->estimated_bytes;
		}
		m_compute_pipelines.erase(lru_it);
	}
}

void MetalPipelineCache::Clear() {
	Common::LockGuard lock(m_mutex);
	m_graphics_pipelines.clear();
	m_compute_pipelines.clear();
	m_total_memory_bytes = 0;
}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
