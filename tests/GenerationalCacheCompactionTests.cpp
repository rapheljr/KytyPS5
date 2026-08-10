// GenerationalCacheCompactionTests.cpp
//
// Unit & Integration Tests for RadixCodeCache Generational LRU Eviction & Compaction.

#include "loader/recompiler/radixCodeCache.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <chrono>
#include <thread>
#include <vector>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

static void DummyBlock1(GuestCpuContext*) {}
static void DummyBlock2(GuestCpuContext*) {}
static void DummyBlock3(GuestCpuContext*) {}

void TestRadixCodeCacheLRUEviction() {
	std::printf("[TEST] RadixCodeCache_LRUEviction\n");

	RadixCodeCache cache;
	Check(cache.GetBlockCount() == 0, "Initial block count should be 0");

	// Insert 3 blocks
	cache.Insert(0x401000, DummyBlock1);
	cache.Insert(0x402000, DummyBlock2);
	cache.Insert(0x403000, DummyBlock3);

	Check(cache.GetBlockCount() == 3, "Expected 3 blocks in cache");
	Check(cache.Lookup(0x401000) == DummyBlock1, "Lookup 0x401000 failed");
	Check(cache.Lookup(0x402000) == DummyBlock2, "Lookup 0x402000 failed");
	Check(cache.Lookup(0x403000) == DummyBlock3, "Lookup 0x403000 failed");

	// Sleep 5ms to create a timestamp delta
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
	uint64_t threshold_ts = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());

	// Access block 3 to update its timestamp
	std::this_thread::sleep_for(std::chrono::milliseconds(2));
	Check(cache.Lookup(0x403000) == DummyBlock3, "Lookup 0x403000 failed");

	// Evict blocks older than threshold_ts (should evict block 1 and block 2, keep block 3)
	size_t evicted = cache.EvictOldestBlocks(threshold_ts, 10);
	Check(evicted == 2, "Expected 2 blocks to be evicted");
	Check(cache.Lookup(0x401000) == nullptr, "Block 1 should be evicted");
	Check(cache.Lookup(0x402000) == nullptr, "Block 2 should be evicted");
	Check(cache.Lookup(0x403000) == DummyBlock3, "Block 3 should remain in cache");
	Check(cache.GetBlockCount() == 1, "Expected 1 active block remaining");

	std::printf("  [ OK ] RadixCodeCache_LRUEviction\n");
}

void TestGenerationalCodeCacheAllocAndCompact() {
	std::printf("[TEST] GenerationalCodeCache_AllocAndCompact\n");

	GenerationalCodeCache gen_cache(1 * 1024 * 1024, 2 * 1024 * 1024);

	// Allocate in Gen0
	uint8_t* b1 = gen_cache.AllocateGen0(128);
	uint8_t* b2 = gen_cache.AllocateGen0(256);
	uint8_t* b3 = gen_cache.AllocateGen0(512);

	Check(b1 != nullptr && b2 != nullptr && b3 != nullptr, "Gen0 allocations failed");
	const auto& stats = gen_cache.GetStats();
	Check(stats.active_block_cnt == 3, "Expected 3 active blocks");
	Check(stats.gen0_bytes_used >= 128 + 256 + 512, "Gen0 bytes used should reflect allocations");

	// Promote block 2 to Gen1
	uint8_t* gen1_b2 = gen_cache.PromoteToGen1(b2, 256);
	Check(gen1_b2 != nullptr, "PromoteToGen1 failed");
	Check(gen_cache.GetStats().gen1_bytes_used >= 256, "Gen1 bytes used should increase");

	// Test LRU reclamation
	size_t reclaimed = gen_cache.EvictLRU(256);
	Check(reclaimed == 256, "Expected 256 bytes reclaimed");

	// Test compaction
	bool compact_ok = gen_cache.CompactCache();
	Check(compact_ok, "CompactCache failed");
	Check(gen_cache.GetStats().gen0_bytes_used == 0, "Gen0 bytes used should be 0 after compaction");

	std::printf("  [ OK ] GenerationalCodeCache_AllocAndCompact\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — RadixCodeCache Generational LRU Eviction Test Suite\n");
	std::printf("================================================================================\n");

	TestRadixCodeCacheLRUEviction();
	TestGenerationalCodeCacheAllocAndCompact();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
