#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALPIPELINECACHE_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALPIPELINECACHE_H_

// MetalPipelineCache — Phase E
//
// Manages creation, caching, lookup, reuse, and LRU eviction of
// MTLRenderPipelineState, MTLDepthStencilState, and MTLComputePipelineState objects.
//
// Features:
//   - High-performance hashing & lookup via C++ std::unordered_map
//   - Reuse of Vulkan pipeline abstraction state (PipelineStaticParameters & PipelineRenderingState)
//   - Deduplicated pipeline compilation (avoids redundant MTLLibrary / MTLPipelineState creation)
//   - Bounded capacity with Least-Recently-Used (LRU) cache eviction
//   - Diagnostic metrics: hit count, miss count, hit rate %, estimated VRAM/RAM usage
//   - Thread-safe lookup & compilation under Common::Mutex

#include "common/common.h"
#include "common/threads.h"
#include "graphics/host_gpu/renderer/pipeline/pipelineCache.h"
#include "graphics/shader/shader.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

// ─────────────────────────────────────────────────────────────────────────────
// Metal Pipeline Key Hashing Utilities
// ─────────────────────────────────────────────────────────────────────────────

struct MetalPipelineKeyHasher {
	static void Mix(std::size_t& hash, std::size_t value) noexcept {
		hash ^= value + static_cast<std::size_t>(0x9e3779b97f4a7c15ull) + (hash << 6u) + (hash >> 2u);
	}

	static void MixShaderId(std::size_t& hash, const ShaderId& id) noexcept {
		Mix(hash, id.hash0);
		Mix(hash, id.crc32);
		Mix(hash, id.ids.size());
		for (auto value : id.ids) {
			Mix(hash, value);
		}
	}

	static void MixStaticParams(std::size_t& hash, const PipelineStaticParameters& params) noexcept {
		const auto* bytes = reinterpret_cast<const uint8_t*>(&params);
		for (std::size_t i = 0; i < sizeof(params); ++i) {
			Mix(hash, bytes[i]);
		}
	}

	static void MixRendering(std::size_t& hash, const PipelineRenderingState& rendering) noexcept {
		Mix(hash, rendering.color_count);
		for (uint32_t i = 0; i < rendering.color_count; ++i) {
			Mix(hash, static_cast<uint32_t>(rendering.color_formats[i]));
		}
		Mix(hash, static_cast<uint32_t>(rendering.depth_format));
		Mix(hash, static_cast<uint32_t>(rendering.stencil_format));
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Metal Graphics Pipeline Key & State
// ─────────────────────────────────────────────────────────────────────────────

struct MetalGraphicsPipelineKey {
	PipelineRenderingState   rendering {};
	ShaderId                 vs_shader_id {};
	ShaderId                 ps_shader_id {};
	PipelineStaticParameters static_params {};
	std::string              msl_vs_code;
	std::string              msl_ps_code;

	bool operator==(const MetalGraphicsPipelineKey& other) const noexcept {
		return rendering == other.rendering &&
		       vs_shader_id == other.vs_shader_id &&
		       ps_shader_id == other.ps_shader_id &&
		       static_params == other.static_params &&
		       msl_vs_code == other.msl_vs_code &&
		       msl_ps_code == other.msl_ps_code;
	}
};

struct MetalGraphicsPipelineKeyHash {
	std::size_t operator()(const MetalGraphicsPipelineKey& key) const noexcept {
		std::size_t hash = 0;
		MetalPipelineKeyHasher::MixRendering(hash, key.rendering);
		MetalPipelineKeyHasher::MixShaderId(hash, key.vs_shader_id);
		MetalPipelineKeyHasher::MixShaderId(hash, key.ps_shader_id);
		MetalPipelineKeyHasher::MixStaticParams(hash, key.static_params);

		for (char c : key.msl_vs_code) {
			MetalPipelineKeyHasher::Mix(hash, static_cast<size_t>(c));
		}
		for (char c : key.msl_ps_code) {
			MetalPipelineKeyHasher::Mix(hash, static_cast<size_t>(c));
		}
		return hash;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Metal Compute Pipeline Key & State
// ─────────────────────────────────────────────────────────────────────────────

struct MetalComputePipelineKey {
	ShaderId    cs_shader_id {};
	std::string msl_cs_code;

	bool operator==(const MetalComputePipelineKey& other) const noexcept {
		return cs_shader_id == other.cs_shader_id && msl_cs_code == other.msl_cs_code;
	}
};

struct MetalComputePipelineKeyHash {
	std::size_t operator()(const MetalComputePipelineKey& key) const noexcept {
		std::size_t hash = 0;
		MetalPipelineKeyHasher::MixShaderId(hash, key.cs_shader_id);
		for (char c : key.msl_cs_code) {
			MetalPipelineKeyHasher::Mix(hash, static_cast<size_t>(c));
		}
		return hash;
	}
};

// ─────────────────────────────────────────────────────────────────────────────
// Cached Pipeline Object Wrappers
// ─────────────────────────────────────────────────────────────────────────────

struct MetalRenderPipelineEntry {
	void*    render_pipeline_state = nullptr; // id<MTLRenderPipelineState>
	void*    depth_stencil_state   = nullptr; // id<MTLDepthStencilState>
	uint64_t last_used_timestamp   = 0;       // For LRU eviction
	uint32_t use_count             = 0;
	size_t   estimated_bytes       = 0;
};

struct MetalComputePipelineEntry {
	void*    compute_pipeline_state = nullptr; // id<MTLComputePipelineState>
	uint64_t last_used_timestamp    = 0;        // For LRU eviction
	uint32_t use_count              = 0;
	size_t   estimated_bytes        = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// MetalPipelineCache Class
// ─────────────────────────────────────────────────────────────────────────────

class MetalPipelineCache final {
public:
	/// Construct cache attached to host Metal device handle (id<MTLDevice>).
	/// Max capacity defaults to 512 entries before LRU eviction triggers.
	explicit MetalPipelineCache(void* mtl_device,
	                           size_t max_graphics_capacity = 512,
	                           size_t max_compute_capacity  = 512);
	~MetalPipelineCache();

	KYTY_CLASS_NO_COPY(MetalPipelineCache);

	// ── Pipeline Retrieval & Compilation ──────────────────────────────────────

	/// Get or compile a graphics pipeline state matching the specified key.
	/// If found in cache, updates LRU timestamp and returns instantly.
	/// If miss, compiles MSL shaders and builds MTLRenderPipelineState.
	MetalRenderPipelineEntry* GetOrCreateGraphicsPipeline(const MetalGraphicsPipelineKey& key);

	/// Get or compile a compute pipeline state matching the specified key.
	MetalComputePipelineEntry* GetOrCreateComputePipeline(const MetalComputePipelineKey& key);

	// ── Cache Eviction & Cleanup ──────────────────────────────────────────────

	/// Manually trigger LRU eviction down to target capacity limits.
	void EvictLRUGraphics(size_t target_capacity);
	void EvictLRUCompute(size_t target_capacity);

	/// Clear all cached graphics & compute pipelines.
	void Clear();

	// ── Diagnostic Metrics ────────────────────────────────────────────────────

	[[nodiscard]] size_t GetGraphicsCacheSize()  const noexcept { return m_graphics_pipelines.size(); }
	[[nodiscard]] size_t GetComputeCacheSize()   const noexcept { return m_compute_pipelines.size(); }

	[[nodiscard]] uint64_t GetGraphicsHits()     const noexcept { return m_graphics_hits; }
	[[nodiscard]] uint64_t GetGraphicsMisses()   const noexcept { return m_graphics_misses; }
	[[nodiscard]] uint64_t GetComputeHits()      const noexcept { return m_compute_hits; }
	[[nodiscard]] uint64_t GetComputeMisses()    const noexcept { return m_compute_misses; }

	[[nodiscard]] double GetGraphicsHitRate() const noexcept {
		const uint64_t total = m_graphics_hits + m_graphics_misses;
		return (total > 0) ? (static_cast<double>(m_graphics_hits) / static_cast<double>(total)) * 100.0 : 0.0;
	}

	[[nodiscard]] double GetComputeHitRate() const noexcept {
		const uint64_t total = m_compute_hits + m_compute_misses;
		return (total > 0) ? (static_cast<double>(m_compute_hits) / static_cast<double>(total)) * 100.0 : 0.0;
	}

	[[nodiscard]] size_t GetTotalEstimatedMemoryUsageBytes() const noexcept {
		return m_total_memory_bytes;
	}

private:
	void*  m_device                 = nullptr; // id<MTLDevice>
	size_t m_max_graphics_capacity = 512;
	size_t m_max_compute_capacity  = 512;

	std::unordered_map<MetalGraphicsPipelineKey, std::unique_ptr<MetalRenderPipelineEntry>, MetalGraphicsPipelineKeyHash>
	    m_graphics_pipelines;

	std::unordered_map<MetalComputePipelineKey, std::unique_ptr<MetalComputePipelineEntry>, MetalComputePipelineKeyHash>
	    m_compute_pipelines;

	uint64_t m_graphics_hits      = 0;
	uint64_t m_graphics_misses    = 0;
	uint64_t m_compute_hits       = 0;
	uint64_t m_compute_misses     = 0;
	uint64_t m_timestamp_counter  = 0;
	size_t   m_total_memory_bytes = 0;

	mutable Common::Mutex m_mutex;

	// Helper compilation methods
	MetalRenderPipelineEntry* CompileGraphicsPipeline(const MetalGraphicsPipelineKey& key);
	MetalComputePipelineEntry* CompileComputePipeline(const MetalComputePipelineKey& key);
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALPIPELINECACHE_H_
