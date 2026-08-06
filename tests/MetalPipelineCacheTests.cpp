// MetalPipelineCacheTests.cpp
//
// Unit & Integration Tests for Metal Pipeline State Cache & LRU Eviction Engine.

#include "graphics/host_gpu/renderer/backend/metalPipelineCache.h"

#include <cstdio>
#include <cstdlib>

using namespace Libs::Graphics;

static void TestMetalGraphicsPipelineCaching() {
	std::printf("[TEST] MetalGraphicsPipelineCaching\n");

	MetalPipelineCache cache(nullptr, 4, 4);

	MetalGraphicsPipelineKey key1;
	key1.rendering.color_count = 1;
	key1.rendering.color_formats[0] = vk::Format::eB8G8R8A8Unorm;
	key1.msl_vs_code = "vertex_shader_code_1";
	key1.msl_ps_code = "fragment_shader_code_1";

	auto* entry1 = cache.GetOrCreateGraphicsPipeline(key1);
	if (!entry1) {
		std::fprintf(stderr, "FAIL: GetOrCreateGraphicsPipeline returned null\n");
		std::exit(1);
	}

	if (cache.GetGraphicsHits() != 0 || cache.GetGraphicsMisses() != 1) {
		std::fprintf(stderr, "FAIL: Graphics hit/miss counter mismatch after first compilation\n");
		std::exit(1);
	}

	// Secondary lookup (cache hit)
	auto* entry1_hit = cache.GetOrCreateGraphicsPipeline(key1);
	if (entry1_hit != entry1) {
		std::fprintf(stderr, "FAIL: GetOrCreateGraphicsPipeline returned different entry pointer on cache hit\n");
		std::exit(1);
	}

	if (cache.GetGraphicsHits() != 1 || cache.GetGraphicsMisses() != 1) {
		std::fprintf(stderr, "FAIL: Graphics hit/miss counter mismatch after cache hit\n");
		std::exit(1);
	}

	if (cache.GetGraphicsHitRate() < 49.0 || cache.GetGraphicsHitRate() > 51.0) {
		std::fprintf(stderr, "FAIL: Hit rate calculation unexpected: %f%%\n", cache.GetGraphicsHitRate());
		std::exit(1);
	}

	std::printf("  [ OK ] MetalGraphicsPipelineCaching\n");
}

static void TestMetalComputePipelineCachingAndEviction() {
	std::printf("[TEST] MetalComputePipelineCachingAndEviction\n");

	MetalPipelineCache cache(nullptr, 2, 2);

	MetalComputePipelineKey key1;
	key1.msl_cs_code = "kernel_1";

	MetalComputePipelineKey key2;
	key2.msl_cs_code = "kernel_2";

	MetalComputePipelineKey key3;
	key3.msl_cs_code = "kernel_3";

	auto* e1 = cache.GetOrCreateComputePipeline(key1);
	auto* e2 = cache.GetOrCreateComputePipeline(key2);

	if (!e1 || !e2 || cache.GetComputeCacheSize() != 2) {
		std::fprintf(stderr, "FAIL: Compute pipeline cache size mismatch\n");
		std::exit(1);
	}

	// Insert third entry — triggers LRU eviction of key1
	auto* e3 = cache.GetOrCreateComputePipeline(key3);
	if (!e3 || cache.GetComputeCacheSize() > 2) {
		std::fprintf(stderr, "FAIL: Compute pipeline cache capacity exceeded max\n");
		std::exit(1);
	}

	if (cache.GetComputeMisses() != 3) {
		std::fprintf(stderr, "FAIL: Compute misses count mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MetalComputePipelineCachingAndEviction\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Metal Pipeline Cache Unit & Integration Test Suite\n");
	std::printf("================================================================================\n");

	TestMetalGraphicsPipelineCaching();
	TestMetalComputePipelineCachingAndEviction();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
