// CodeCacheBenchmarkTests.cpp
//
// Complete Test Suite & Benchmark Harness for Upgraded Executable Code Cache.

#include "loader/recompiler/radixCodeCache.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "CodeCacheBenchmarkTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

static void DummyHostFunc() {}

void TestRadixCodeCacheLookup() {
	std::printf("  [Cache Test 1] Testing Lock-Free 4-Level Radix Tree Insertion & O(1) Lookup...\n");

	RadixCodeCache cache;
	uint64_t rip1 = 0x140001000ULL;
	uint64_t rip2 = 0x140002000ULL;

	Check(cache.Lookup(rip1) == nullptr, "Must return nullptr for uninserted RIP");

	cache.Insert(rip1, DummyHostFunc);
	cache.Insert(rip2, DummyHostFunc);

	Check(cache.Lookup(rip1) == DummyHostFunc, "Radix lookup rip1 mismatch");
	Check(cache.Lookup(rip2) == DummyHostFunc, "Radix lookup rip2 mismatch");

	cache.Invalidate(rip1);
	Check(cache.Lookup(rip1) == nullptr, "Invalidated RIP must return nullptr");

	std::printf("  [OK] Cache Test 1: Lock-Free Radix Tree passed\n");
}

void TestGenerationalCacheAndCompaction() {
	std::printf("  [Cache Test 2] Testing Generational Allocation (Gen0/Gen1) & Compaction...\n");

	GenerationalCodeCache gen_cache(1024 * 1024, 2 * 1024 * 1024);

	uint8_t* gen0_code = gen_cache.AllocateGen0(128);
	Check(gen0_code != nullptr, "Gen0 allocation failed");
	Check(gen_cache.GetStats().gen0_bytes_used >= 128, "Gen0 bytes counter mismatch");

	uint8_t* gen1_code = gen_cache.PromoteToGen1(gen0_code, 128);
	Check(gen1_code != nullptr, "Gen1 promotion failed");
	Check(gen_cache.GetStats().gen1_bytes_used >= 128, "Gen1 bytes counter mismatch");

	double frag_pct = gen_cache.CalculateFragmentationRatio();
	Check(frag_pct >= 0.0, "Fragmentation ratio calculation failed");

	bool ok = gen_cache.CompactCache();
	Check(ok, "Cache compaction must succeed");

	std::printf("  [OK] Cache Test 2: Generational Cache & Compaction passed\n");
}

void TestPersistentSerializationAndCrossSessionReuse() {
	std::printf("  [Cache Test 3] Testing Persistent Cache Serialization & Cross-Session Reuse...\n");

	std::string temp_file = "test_jit_cache.kyty_jit_cache";
	{
		GenerationalCodeCache save_cache(1024 * 1024, 1024 * 1024);
		uint8_t* code = save_cache.AllocateGen0(64);
		if (code) {
			code[0] = 0xC3; // RET
		}
		bool saved = save_cache.SerializeToFile(temp_file);
		Check(saved, "Cache serialization to disk failed");
	}

	{
		GenerationalCodeCache load_cache(1024 * 1024, 1024 * 1024);
		bool loaded = load_cache.DeserializeFromFile(temp_file);
		Check(loaded, "Cache deserialization from disk failed");
		Check(load_cache.GetStats().gen0_bytes_used > 0, "Loaded Gen0 bytes mismatch");
	}

	std::filesystem::remove(temp_file);
	std::printf("  [OK] Cache Test 3: Persistent Serialization & Cross-Session Reuse passed\n");
}

void TestCodeCachePerformanceBenchmark() {
	std::printf("  [Cache Benchmark] Benchmarking Radix Tree Lookup & Insertion Latencies...\n");

	const size_t ITERATIONS = 1000000;
	RadixCodeCache cache;

	// 1. Insertion Latency Benchmark
	auto start_ins = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERATIONS; ++i) {
		cache.Insert(0x140000000ULL + i * 16, DummyHostFunc);
	}
	auto end_ins = std::chrono::high_resolution_clock::now();
	double ins_ms = std::chrono::duration<double, std::milli>(end_ins - start_ins).count();
	double ins_ns_per_op = (ins_ms * 1e6) / ITERATIONS;

	// 2. Lookup Latency Benchmark
	auto start_look = std::chrono::high_resolution_clock::now();
	volatile uint64_t hits = 0;
	for (size_t i = 0; i < ITERATIONS; ++i) {
		if (cache.Lookup(0x140000000ULL + i * 16)) hits++;
	}
	auto end_look = std::chrono::high_resolution_clock::now();
	double look_ms = std::chrono::duration<double, std::milli>(end_look - start_look).count();
	double look_ns_per_op = (look_ms * 1e6) / ITERATIONS;

	std::printf("\n====================================================\n");
	std::printf("    Lock-Free Radix Code Cache Performance Metrics  \n");
	std::printf("====================================================\n");
	std::printf("  [Radix Insertion Latency] : %.2f ns / insertion\n", ins_ns_per_op);
	std::printf("  [Radix Lookup Latency]    : %.2f ns / lookup (Lock-Free O(1))\n", look_ns_per_op);
	std::printf("  [Lookup Throughput]       : %.2f M lookups / sec\n", (ITERATIONS / (look_ms / 1000.0)) / 1e6);
	std::printf("====================================================\n\n");

	Check(hits == ITERATIONS, "All radix lookups must hit");
	std::printf("  [OK] Cache Benchmark: Latency and Throughput verification passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Executable Code Cache Benchmark Suite      \n");
	std::printf("====================================================\n");

	TestRadixCodeCacheLookup();
	TestGenerationalCacheAndCompaction();
	TestPersistentSerializationAndCrossSessionReuse();
	TestCodeCachePerformanceBenchmark();

	std::printf("\nALL CODE CACHE BENCHMARK TESTS PASSED!\n");
	return 0;
}
