// MetalSparseHeapTests.mm
//
// Unit & Integration tests for Metal 3 Sparse Resource Heap Subsystem.

#include "graphics/host_gpu/renderer/backend/metalSparseResourceHeap.h"

#import <Metal/Metal.h>

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::Graphics;

void TestSparseHeapConfiguration() {
	std::printf("[TEST] Metal Sparse Heap Configuration & Stats...\n");

	MetalSparseResourceHeap heap;
	MetalSparseHeapConfig config;
	config.total_virtual_size = 128 * 1024 * 1024; // 128 MB
	config.tile_size_bytes = 64 * 1024;           // 64 KB per tile
	config.allow_sparse_tier = true;

	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device) {
		std::printf("  [SKIP] Metal device not available in headless test environment\n");
		return;
	}

	bool init_res = heap.Initialize((__bridge void*)device, config);
	Check(init_res, "Sparse heap initialization failed");

	const auto& stats = heap.GetStats();
	Check(stats.total_virtual_bytes == 128 * 1024 * 1024, "Virtual bytes mismatch");
	Check(stats.total_tiles == (128 * 1024 * 1024) / (64 * 1024), "Total tiles mismatch");
	Check(stats.resident_bytes == 0, "Initial resident bytes should be 0");
	Check(stats.mapped_tiles == 0, "Initial mapped tiles should be 0");

	// 1. Map tiles
	bool map1 = heap.MapTile(0, 0x100000);
	bool map2 = heap.MapTile(1, 0x110000);
	Check(map1 && map2, "Mapping tiles failed");
	Check(heap.IsTileResident(0), "Tile 0 should be resident");
	Check(heap.IsTileResident(1), "Tile 1 should be resident");
	Check(!heap.IsTileResident(2), "Tile 2 should not be resident");

	Check(heap.GetStats().mapped_tiles == 2, "Mapped tiles count mismatch");
	Check(heap.GetStats().resident_bytes == 128 * 1024, "Resident bytes mismatch");

	// 2. Unmap tile
	bool unmap1 = heap.UnmapTile(0);
	Check(unmap1, "Unmapping tile 0 failed");
	Check(!heap.IsTileResident(0), "Tile 0 should no longer be resident");
	Check(heap.GetStats().mapped_tiles == 1, "Mapped tiles count after unmap mismatch");

	heap.Shutdown();
	Check(heap.GetStats().resident_bytes == 0, "Resident bytes after shutdown should be 0");

	[device release];
	std::printf("  [OK] Metal Sparse Heap Configuration & Stats\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal 3 Sparse Resource Heap Test Suite\n");
	std::printf("================================================================================\n");

	TestSparseHeapConfiguration();

	std::printf("================================================================================\n");
	std::printf("  Results: All Metal Sparse Heap Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
