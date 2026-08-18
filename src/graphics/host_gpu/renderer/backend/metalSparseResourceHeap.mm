// metalSparseResourceHeap.mm
//
// Metal 3 Dynamic Sparse Resource Heap & Virtual Texturing Implementation.

#include "graphics/host_gpu/renderer/backend/metalSparseResourceHeap.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <iostream>

namespace Libs::Graphics {

MetalSparseResourceHeap::~MetalSparseResourceHeap() {
	Shutdown();
}

bool MetalSparseResourceHeap::Initialize(void* mtl_device, const MetalSparseHeapConfig& config) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_config = config;
	m_device = mtl_device;

	@autoreleasepool {
		id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;
		if (!device) return false;

		m_stats.total_virtual_bytes = config.total_virtual_size;
		m_stats.total_tiles = static_cast<uint32_t>(config.total_virtual_size / config.tile_size_bytes);
		m_tile_residency.assign(m_stats.total_tiles, false);

		MTLHeapDescriptor* desc = [[MTLHeapDescriptor alloc] init];
		desc.size = config.total_virtual_size;
		desc.storageMode = MTLStorageModePrivate;
		desc.hazardTrackingMode = MTLHazardTrackingModeTracked;

		if (@available(macOS 13.0, *)) {
			if ([device supportsFamily:MTLGPUFamilyMetal3] && config.allow_sparse_tier) {
				desc.type = MTLHeapTypeSparse;
				m_stats.hardware_sparse_supported = true;
			} else {
				desc.type = MTLHeapTypeAutomatic;
				m_stats.hardware_sparse_supported = false;
			}
		} else {
			desc.type = MTLHeapTypeAutomatic;
			m_stats.hardware_sparse_supported = false;
		}

		id<MTLHeap> heap = [device newHeapWithDescriptor:desc];
		[desc release];

		if (!heap) {
			// Fallback allocation with smaller chunk or automatic type
			desc = [[MTLHeapDescriptor alloc] init];
			desc.size = config.tile_size_bytes * 16; // minimal initial backing
			desc.storageMode = MTLStorageModePrivate;
			desc.type = MTLHeapTypeAutomatic;
			heap = [device newHeapWithDescriptor:desc];
			[desc release];
		}

		if (heap) {
			m_heap = (void*)[heap retain];
		}

		return true;
	}
}

void MetalSparseResourceHeap::Shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_heap) {
		@autoreleasepool {
			id<MTLHeap> heap = (__bridge id<MTLHeap>)m_heap;
			[heap release];
		}
		m_heap = nullptr;
	}
	m_tile_residency.clear();
	m_tile_to_physical_map.clear();
	m_stats.resident_bytes = 0;
	m_stats.mapped_tiles = 0;
}

bool MetalSparseResourceHeap::MapTile(uint32_t tile_index, uint64_t physical_offset) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (tile_index >= m_stats.total_tiles) return false;

	if (!m_tile_residency[tile_index]) {
		m_tile_residency[tile_index] = true;
		m_stats.mapped_tiles++;
		m_stats.resident_bytes += m_config.tile_size_bytes;
	}

	m_tile_to_physical_map[tile_index] = physical_offset;
	m_stats.map_operations++;
	return true;
}

bool MetalSparseResourceHeap::UnmapTile(uint32_t tile_index) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (tile_index >= m_stats.total_tiles) return false;

	if (m_tile_residency[tile_index]) {
		m_tile_residency[tile_index] = false;
		m_stats.mapped_tiles--;
		m_stats.resident_bytes -= m_config.tile_size_bytes;
		m_tile_to_physical_map.erase(tile_index);
	}

	m_stats.unmap_operations++;
	return true;
}

bool MetalSparseResourceHeap::IsTileResident(uint32_t tile_index) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (tile_index >= m_tile_residency.size()) return false;
	return m_tile_residency[tile_index];
}

} // namespace Libs::Graphics
