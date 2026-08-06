// radixCodeCache.h
//
// Lock-Free Radix Tree Executable Code Cache, Generational LRU Eviction & Persistent Serializer.

#ifndef LOADER_RECOMPILER_RADIX_CODE_CACHE_H
#define LOADER_RECOMPILER_RADIX_CODE_CACHE_H

#include "common/common.h"
#include "loader/recompiler/x86BlockCache.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace Loader::Recompiler {

struct RadixNode {
	std::atomic<RadixNode*>         children[256]{};
	std::atomic<CompiledBlockFunc> leaf_func{nullptr};
	std::atomic<uint64_t>           last_access_timestamp{0};
};

class RadixCodeCache {
public:
	RadixCodeCache();
	~RadixCodeCache();

	KYTY_CLASS_NO_COPY(RadixCodeCache);

	void Insert(uint64_t guest_rip, CompiledBlockFunc host_func) noexcept;
	[[nodiscard]] CompiledBlockFunc Lookup(uint64_t guest_rip) noexcept;
	[[nodiscard]] CompiledBlockFunc LookupDirect(uint64_t guest_rip) noexcept;
	void Invalidate(uint64_t guest_rip) noexcept;
	void Clear() noexcept;

private:
	void FreeNodeRecursive(RadixNode* node);

	RadixNode* m_root = nullptr;
};

struct CacheGenerationStats {
	size_t gen0_bytes_used  = 0;
	size_t gen1_bytes_used  = 0;
	size_t active_block_cnt = 0;
	double fragmentation_pct = 0.0;
};

class GenerationalCodeCache {
public:
	explicit GenerationalCodeCache(size_t gen0_capacity = 8 * 1024 * 1024, size_t gen1_capacity = 16 * 1024 * 1024);
	~GenerationalCodeCache();

	KYTY_CLASS_NO_COPY(GenerationalCodeCache);

	uint8_t* AllocateGen0(size_t size_bytes);
	uint8_t* PromoteToGen1(uint8_t* gen0_ptr, size_t size_bytes);

	double CalculateFragmentationRatio() const noexcept;
	bool CompactCache() noexcept;

	// Persistent Cache Serialization & Cross-Session Reuse
	bool SerializeToFile(const std::string& filepath) const;
	bool DeserializeFromFile(const std::string& filepath);

	[[nodiscard]] const CacheGenerationStats& GetStats() const noexcept { return m_stats; }

private:
	size_t m_gen0_capacity = 0;
	size_t m_gen1_capacity = 0;
	uint8_t* m_gen0_base   = nullptr;
	uint8_t* m_gen1_base   = nullptr;

	std::atomic<size_t> m_gen0_offset{0};
	std::atomic<size_t> m_gen1_offset{0};

	mutable CacheGenerationStats m_stats{};
	mutable std::mutex           m_gen_mutex;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_RADIX_CODE_CACHE_H
