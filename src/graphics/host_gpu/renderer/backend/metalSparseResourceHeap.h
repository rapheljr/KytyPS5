// metalSparseResourceHeap.h
//
// Metal 3 Dynamic Sparse Resource Heap & Virtual Texturing Subsystem for macOS Apple Silicon.

#ifndef GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SPARSE_RESOURCE_HEAP_H
#define GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SPARSE_RESOURCE_HEAP_H

#include "common/common.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

struct MetalSparseHeapConfig {
	uint64_t total_virtual_size = 256 * 1024 * 1024; // 256 MB
	uint32_t tile_size_bytes    = 64 * 1024;          // 64 KB per sparse tile
	bool     allow_sparse_tier  = true;
};

struct MetalSparseHeapStats {
	uint64_t total_virtual_bytes  = 0;
	uint64_t resident_bytes       = 0;
	uint32_t total_tiles          = 0;
	uint32_t mapped_tiles         = 0;
	uint32_t map_operations       = 0;
	uint32_t unmap_operations     = 0;
	bool     hardware_sparse_supported = false;
};

class MetalSparseResourceHeap {
public:
	MetalSparseResourceHeap() = default;
	~MetalSparseResourceHeap();

	KYTY_CLASS_NO_COPY(MetalSparseResourceHeap);

	bool Initialize(void* mtl_device, const MetalSparseHeapConfig& config);
	void Shutdown();

	bool MapTile(uint32_t tile_index, uint64_t physical_offset);
	bool UnmapTile(uint32_t tile_index);
	bool IsTileResident(uint32_t tile_index) const;

	void* GetNativeHeap() const noexcept { return m_heap; }
	const MetalSparseHeapStats& GetStats() const noexcept { return m_stats; }
	const MetalSparseHeapConfig& GetConfig() const noexcept { return m_config; }

private:
	void*                 m_device = nullptr;
	void*                 m_heap   = nullptr;
	MetalSparseHeapConfig m_config;
	MetalSparseHeapStats  m_stats;
	mutable std::mutex    m_mutex;

	std::vector<bool>     m_tile_residency;
	std::unordered_map<uint32_t, uint64_t> m_tile_to_physical_map;
};

} // namespace Libs::Graphics

#endif // GRAPHICS_HOST_GPU_RENDERER_BACKEND_METAL_SPARSE_RESOURCE_HEAP_H
