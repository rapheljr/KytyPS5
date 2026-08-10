// MetalUnifiedMemoryTrackingTests.mm
//
// Unit & Integration Tests for Metal Unified Memory Dirty Page Tracking.

#include "graphics/host_gpu/renderer/backend/metalUnifiedMemoryTracker.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Graphics::HostGpu;

static void TestUnifiedMemoryLifecycle() {
	std::printf("[TEST] UnifiedMemory_Lifecycle\n");

	MetalUnifiedMemoryTracker tracker;
	if (!tracker.Initialize()) {
		std::fprintf(stderr, "FAIL: MetalUnifiedMemoryTracker initialization failed\n");
		std::exit(1);
	}

	if (!tracker.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned false\n");
		std::exit(1);
	}

	tracker.Shutdown();
	if (tracker.IsInitialized()) {
		std::fprintf(stderr, "FAIL: IsInitialized returned true after shutdown\n");
		std::exit(1);
	}

	std::printf("  [ OK ] UnifiedMemory_Lifecycle\n");
}

static void TestDirtyRangeTrackingAndSync() {
	std::printf("[TEST] UnifiedMemory_DirtyRangeTrackingAndSync\n");

	MetalUnifiedMemoryTracker tracker;
	tracker.Initialize();

	// Allocate 64KB host buffer (16 pages)
	std::vector<uint8_t> host_buffer(65536, 0);
	uint32_t handle = tracker.RegisterBuffer(0x80000000ULL, host_buffer.data(), host_buffer.size());

	if (handle == 0) {
		std::fprintf(stderr, "FAIL: RegisterBuffer returned 0\n");
		std::exit(1);
	}

	// Mark pages 2 to 4 dirty (offset 8192, size 12288 -> 3 pages)
	tracker.MarkRangeDirty(handle, 8192, 12288);
	tracker.SynchronizeDirtyRanges(handle);

	const auto& stats = tracker.GetStats();
	if (stats.total_dirty_pages_flushed != 3 || stats.sync_barrier_count != 1) {
		std::fprintf(stderr, "FAIL: Dirty page count mismatch (Flushed=%llu, Syncs=%u)\n",
		             stats.total_dirty_pages_flushed, stats.sync_barrier_count);
		std::exit(1);
	}

	// Subsequent sync without new writes should flush 0 pages
	tracker.SynchronizeDirtyRanges(handle);
	if (tracker.GetStats().total_dirty_pages_flushed != 3 || tracker.GetStats().sync_barrier_count != 2) {
		std::fprintf(stderr, "FAIL: Subsequent sync flushed unexpected pages\n");
		std::exit(1);
	}

	tracker.Shutdown();
	std::printf("  [ OK ] UnifiedMemory_DirtyRangeTrackingAndSync\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal Unified Memory Dirty Tracking Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestUnifiedMemoryLifecycle();
	TestDirtyRangeTrackingAndSync();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
