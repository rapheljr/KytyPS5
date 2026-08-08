// x86BlockCache.h
//
// Lock-Free Guest RIP Code Cache & Memory Allocator for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_BLOCK_CACHE_H
#define LOADER_RECOMPILER_X86_BLOCK_CACHE_H

#include "common/common.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace Loader::Recompiler {

using CompiledBlockFunc = void (*)();

struct BlockCacheStats {
	std::atomic<uint32_t> blocks_compiled{0};
	std::atomic<uint32_t> cache_hits{0};
	std::atomic<uint32_t> cache_misses{0};
	std::atomic<size_t>   code_bytes_used{0};
	std::atomic<size_t>   code_bytes_alloc{0};
};


class Arm64CodeCache {
public:
	explicit Arm64CodeCache(size_t capacity_bytes = 16 * 1024 * 1024);
	~Arm64CodeCache();

	KYTY_CLASS_NO_COPY(Arm64CodeCache);

	[[nodiscard]] uint8_t* AllocateCode(size_t size_bytes);
	void InvalidateCache() noexcept;

	static void SetJitWriteProtect(bool enabled) noexcept;
	static void FlushInstructionCache(const void* ptr, size_t size_bytes) noexcept;

private:
	uint8_t* m_base_ptr       = nullptr;
	size_t   m_capacity_bytes = 0;
	std::atomic<size_t> m_offset_bytes{0};
};

class X86BlockCache {
public:
	explicit X86BlockCache(size_t capacity_entries = 65536);
	~X86BlockCache();

	KYTY_CLASS_NO_COPY(X86BlockCache);

	void Insert(uint64_t guest_rip, CompiledBlockFunc host_func) noexcept;
	[[nodiscard]] CompiledBlockFunc Lookup(uint64_t guest_rip) noexcept;
	void Invalidate(uint64_t guest_rip) noexcept;
	void Clear() noexcept;

	[[nodiscard]] uint32_t GetExecutionCount(uint64_t guest_rip) const noexcept;
	[[nodiscard]] bool IsHotBlock(uint64_t guest_rip, uint32_t threshold = 50) const noexcept;
	bool ChainDirectBranch(uint8_t* patch_site, const void* target_host_addr) noexcept;

	[[nodiscard]] const BlockCacheStats& GetStats() const noexcept { return m_stats; }

private:
	struct HashEntry {
		std::atomic<uint64_t>          guest_rip{0};
		std::atomic<CompiledBlockFunc> host_func{nullptr};
		std::atomic<uint32_t>          exec_count{0};
		std::atomic<bool>              is_chained{false};
	};

	size_t m_capacity_mask = 0;
	HashEntry* m_table    = nullptr;
	BlockCacheStats m_stats{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_BLOCK_CACHE_H
