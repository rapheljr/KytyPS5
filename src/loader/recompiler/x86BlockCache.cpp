// x86BlockCache.cpp
//
// Lock-Free Guest RIP Code Cache & Memory Allocator for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86BlockCache.h"

#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <sys/mman.h>

#include <cstdlib>
#include <cstring>

#include "loader/recompiler/blockLinker.h"

namespace Loader::Recompiler {

Arm64CodeCache::Arm64CodeCache(size_t capacity_bytes) : m_capacity_bytes(capacity_bytes) {
	int flags = MAP_ANON | MAP_PRIVATE;
#if defined(MAP_JIT)
	flags |= MAP_JIT;
#endif

	void* ptr = ::mmap(nullptr, m_capacity_bytes, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0);
	if (ptr != MAP_FAILED) {
		m_base_ptr = static_cast<uint8_t*>(ptr);
	} else {
		m_base_ptr = nullptr;
	}
}

Arm64CodeCache::~Arm64CodeCache() {
	if (m_base_ptr) {
		::munmap(m_base_ptr, m_capacity_bytes);
		m_base_ptr = nullptr;
	}
}

uint8_t* Arm64CodeCache::AllocateCode(size_t size_bytes) {
	if (!m_base_ptr || size_bytes == 0) return nullptr;

	size_t aligned_size = (size_bytes + 15u) & ~15u;
	size_t current_offset = m_offset_bytes.fetch_add(aligned_size, std::memory_order_relaxed);

	if (current_offset + aligned_size > m_capacity_bytes) {
		return nullptr; // Out of code cache memory
	}

	return m_base_ptr + current_offset;
}

void Arm64CodeCache::InvalidateCache() noexcept {
	m_offset_bytes.store(0, std::memory_order_relaxed);
}

void Arm64CodeCache::SetJitWriteProtect(bool enabled) noexcept {
#if defined(__APPLE__) && defined(__aarch64__)
	pthread_jit_write_protect_np(enabled ? 1 : 0);
#else
	(void)enabled;
#endif
}

void Arm64CodeCache::FlushInstructionCache(const void* ptr, size_t size_bytes) noexcept {
	if (ptr && size_bytes > 0) {
#if defined(__APPLE__)
		sys_icache_invalidate(const_cast<void*>(ptr), size_bytes);
#endif
	}
}

// ─── Lock-Free Hash Block Cache ───────────────────────────────────────────────

X86BlockCache::X86BlockCache(size_t capacity_entries) {
	size_t cap = 1;
	while (cap < capacity_entries) cap <<= 1;
	m_capacity_mask = cap - 1;
	m_table = new HashEntry[cap];
}

X86BlockCache::~X86BlockCache() {
	delete[] m_table;
	m_table = nullptr;
}

void X86BlockCache::Insert(uint64_t guest_rip, CompiledBlockFunc host_func) noexcept {
	if (!m_table || guest_rip == 0 || !host_func) return;

	size_t slot = (guest_rip ^ (guest_rip >> 12u)) & m_capacity_mask;
	m_table[slot].guest_rip.store(guest_rip, std::memory_order_release);
	m_table[slot].host_func.store(host_func, std::memory_order_release);
	m_table[slot].exec_count.store(1, std::memory_order_release);
	m_table[slot].is_chained.store(false, std::memory_order_release);
	m_stats.blocks_compiled.fetch_add(1, std::memory_order_relaxed);
}

CompiledBlockFunc X86BlockCache::Lookup(uint64_t guest_rip) noexcept {
	if (!m_table || guest_rip == 0) return nullptr;

	size_t slot = (guest_rip ^ (guest_rip >> 12u)) & m_capacity_mask;
	uint64_t cached_rip = m_table[slot].guest_rip.load(std::memory_order_acquire);

	if (cached_rip == guest_rip) {
		m_stats.cache_hits.fetch_add(1, std::memory_order_relaxed);
		m_table[slot].exec_count.fetch_add(1, std::memory_order_relaxed);
		return m_table[slot].host_func.load(std::memory_order_relaxed);
	}

	m_stats.cache_misses.fetch_add(1, std::memory_order_relaxed);
	return nullptr;
}

uint32_t X86BlockCache::GetExecutionCount(uint64_t guest_rip) const noexcept {
	if (!m_table || guest_rip == 0) return 0;
	size_t slot = (guest_rip ^ (guest_rip >> 12u)) & m_capacity_mask;
	if (m_table[slot].guest_rip.load(std::memory_order_relaxed) == guest_rip) {
		return m_table[slot].exec_count.load(std::memory_order_relaxed);
	}
	return 0;
}

bool X86BlockCache::IsHotBlock(uint64_t guest_rip, uint32_t threshold) const noexcept {
	return GetExecutionCount(guest_rip) >= threshold;
}

bool X86BlockCache::ChainDirectBranch(uint8_t* patch_site, const void* target_host_addr) noexcept {
	if (!patch_site || !target_host_addr) return false;
	return BlockLinker::PatchBranchTarget(patch_site, target_host_addr, LinkType::DirectJump);
}

void X86BlockCache::Invalidate(uint64_t guest_rip) noexcept {
	if (!m_table || guest_rip == 0) return;

	size_t slot = (guest_rip ^ (guest_rip >> 12u)) & m_capacity_mask;
	if (m_table[slot].guest_rip.load(std::memory_order_relaxed) == guest_rip) {
		m_table[slot].guest_rip.store(0, std::memory_order_release);
		m_table[slot].host_func.store(nullptr, std::memory_order_release);
		m_table[slot].exec_count.store(0, std::memory_order_release);
		m_table[slot].is_chained.store(false, std::memory_order_release);
	}
}

void X86BlockCache::Clear() noexcept {
	if (!m_table) return;

	for (size_t i = 0; i <= m_capacity_mask; ++i) {
		m_table[i].guest_rip.store(0, std::memory_order_relaxed);
		m_table[i].host_func.store(nullptr, std::memory_order_relaxed);
		m_table[i].exec_count.store(0, std::memory_order_relaxed);
		m_table[i].is_chained.store(false, std::memory_order_relaxed);
	}
}

} // namespace Loader::Recompiler
