// RadixCodeCacheTests.cpp
//
// Unit & Integration Tests for RadixCodeCache Direct Lockless Indexing Engine.

#include "loader/recompiler/radixCodeCache.h"

#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static uint64_t g_dummy_exec_cnt = 0;
static void DummyBlockFunc() {
	g_dummy_exec_cnt++;
}

static void TestRadixCodeCacheDirectLookup() {
	std::printf("[TEST] RadixCodeCacheDirectLookup\n");

	RadixCodeCache cache;

	uint64_t rip1 = 0x400100;
	uint64_t rip2 = 0x800200;

	// Misses before insert
	if (cache.LookupDirect(rip1) != nullptr || cache.LookupDirect(rip2) != nullptr) {
		std::fprintf(stderr, "FAIL: LookupDirect returned non-null before insertion\n");
		std::exit(1);
	}

	cache.Insert(rip1, &DummyBlockFunc);
	cache.Insert(rip2, &DummyBlockFunc);

	// Hits after insert
	auto fn1 = cache.LookupDirect(rip1);
	auto fn2 = cache.LookupDirect(rip2);

	if (fn1 != &DummyBlockFunc || fn2 != &DummyBlockFunc) {
		std::fprintf(stderr, "FAIL: LookupDirect returned wrong function pointer\n");
		std::exit(1);
	}

	// Execution check
	fn1();
	if (g_dummy_exec_cnt != 1) {
		std::fprintf(stderr, "FAIL: Executed block function count mismatch\n");
		std::exit(1);
	}

	// Invalidation
	cache.Invalidate(rip1);
	if (cache.LookupDirect(rip1) != nullptr) {
		std::fprintf(stderr, "FAIL: LookupDirect returned non-null after invalidation\n");
		std::exit(1);
	}

	if (cache.LookupDirect(rip2) != &DummyBlockFunc) {
		std::fprintf(stderr, "FAIL: Invalidation affected wrong block RIP\n");
		std::exit(1);
	}

	std::printf("  [ OK ] RadixCodeCacheDirectLookup\n");
}

static void TestGenerationalCodeCacheAllocation() {
	std::printf("[TEST] GenerationalCodeCacheAllocation\n");

	GenerationalCodeCache gen_cache(65536, 131072);

	uint8_t* ptr1 = gen_cache.AllocateGen0(128);
	if (!ptr1) {
		std::fprintf(stderr, "FAIL: AllocateGen0 returned null\n");
		std::exit(1);
	}

	uint8_t* ptr2 = gen_cache.PromoteToGen1(ptr1, 128);
	if (!ptr2) {
		std::fprintf(stderr, "FAIL: PromoteToGen1 returned null\n");
		std::exit(1);
	}

	double frag = gen_cache.CalculateFragmentationRatio();
	if (frag < 0.0 || frag > 100.0) {
		std::fprintf(stderr, "FAIL: CalculateFragmentationRatio out of range: %f\n", frag);
		std::exit(1);
	}

	std::printf("  [ OK ] GenerationalCodeCacheAllocation\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Lockless Radix Code Cache Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestRadixCodeCacheDirectLookup();
	TestGenerationalCodeCacheAllocation();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
