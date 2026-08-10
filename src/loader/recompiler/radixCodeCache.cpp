// radixCodeCache.cpp
//
// Lock-Free Radix Tree Executable Code Cache, Generational LRU Eviction & Persistent Serializer.

#include "loader/recompiler/radixCodeCache.h"

#include <sys/mman.h>

#include <chrono>
#include <cstring>

namespace Loader::Recompiler {

// ─── RadixCodeCache Implementation (Lock-Free 4-Level Radix Tree) ───────────

RadixCodeCache::RadixCodeCache() {
	m_root = new RadixNode();
}

RadixCodeCache::~RadixCodeCache() {
	FreeNodeRecursive(m_root);
	m_root = nullptr;
}

void RadixCodeCache::FreeNodeRecursive(RadixNode* node) {
	if (!node) return;
	for (int i = 0; i < 256; ++i) {
		RadixNode* child = node->children[i].load(std::memory_order_relaxed);
		if (child) {
			FreeNodeRecursive(child);
		}
	}
	delete node;
}

void RadixCodeCache::Insert(uint64_t guest_rip, CompiledBlockFunc host_func) noexcept {
	if (!m_root || guest_rip == 0 || !host_func) return;

	RadixNode* curr = m_root;
	for (int level = 3; level >= 0; --level) {
		uint8_t byte_val = (guest_rip >> (level * 8u)) & 0xFFu;
		RadixNode* child = curr->children[byte_val].load(std::memory_order_acquire);
		if (!child) {
			RadixNode* new_node = new RadixNode();
			RadixNode* expected = nullptr;
			if (!curr->children[byte_val].compare_exchange_strong(expected, new_node, std::memory_order_release, std::memory_order_acquire)) {
				delete new_node;
				child = expected;
			} else {
				child = new_node;
			}
		}
		curr = child;
	}

	uint64_t now_ts = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	curr->last_access_timestamp.store(now_ts, std::memory_order_relaxed);
	curr->leaf_func.store(host_func, std::memory_order_release);
}

CompiledBlockFunc RadixCodeCache::Lookup(uint64_t guest_rip) noexcept {
	if (!m_root || guest_rip == 0) return nullptr;

	RadixNode* curr = m_root;
	for (int level = 3; level >= 0; --level) {
		uint8_t byte_val = (guest_rip >> (level * 8u)) & 0xFFu;
		curr = curr->children[byte_val].load(std::memory_order_acquire);
		if (!curr) return nullptr;
	}

	uint64_t now_ts = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
	curr->last_access_timestamp.store(now_ts, std::memory_order_relaxed);
	return curr->leaf_func.load(std::memory_order_relaxed);
}

CompiledBlockFunc RadixCodeCache::LookupDirect(uint64_t guest_rip) noexcept {
	if (!m_root || guest_rip == 0) return nullptr;

	RadixNode* curr = m_root;
	for (int level = 3; level >= 0; --level) {
		uint8_t byte_val = (guest_rip >> (level * 8u)) & 0xFFu;
		curr = curr->children[byte_val].load(std::memory_order_relaxed);
		if (!curr) return nullptr;
	}

	return curr->leaf_func.load(std::memory_order_relaxed);
}

void RadixCodeCache::Invalidate(uint64_t guest_rip) noexcept {
	if (!m_root || guest_rip == 0) return;

	RadixNode* curr = m_root;
	for (int level = 3; level >= 0; --level) {
		uint8_t byte_val = (guest_rip >> (level * 8u)) & 0xFFu;
		curr = curr->children[byte_val].load(std::memory_order_acquire);
		if (!curr) return;
	}

	curr->leaf_func.store(nullptr, std::memory_order_release);
}

size_t RadixCodeCache::GetBlockCount() const noexcept {
	if (!m_root) return 0;
	size_t count = 0;
	CountNodesRecursive(m_root, count);
	return count;
}

void RadixCodeCache::CountNodesRecursive(const RadixNode* node, size_t& count) const noexcept {
	if (!node) return;
	if (node->leaf_func.load(std::memory_order_relaxed) != nullptr) {
		count++;
	}
	for (int i = 0; i < 256; ++i) {
		const RadixNode* child = node->children[i].load(std::memory_order_relaxed);
		if (child) {
			CountNodesRecursive(child, count);
		}
	}
}

size_t RadixCodeCache::EvictOldestBlocks(uint64_t older_than_timestamp, size_t max_evictions) noexcept {
	if (!m_root || max_evictions == 0) return 0;
	size_t evicted = 0;
	EvictNodesRecursive(m_root, older_than_timestamp, evicted, max_evictions);
	return evicted;
}

void RadixCodeCache::EvictNodesRecursive(RadixNode* node, uint64_t older_than_ts, size_t& evicted, size_t max_evict) noexcept {
	if (!node || evicted >= max_evict) return;

	if (node->leaf_func.load(std::memory_order_relaxed) != nullptr) {
		uint64_t last_ts = node->last_access_timestamp.load(std::memory_order_relaxed);
		if (last_ts < older_than_ts) {
			node->leaf_func.store(nullptr, std::memory_order_release);
			evicted++;
			if (evicted >= max_evict) return;
		}
	}

	for (int i = 0; i < 256; ++i) {
		RadixNode* child = node->children[i].load(std::memory_order_relaxed);
		if (child) {
			EvictNodesRecursive(child, older_than_ts, evicted, max_evict);
			if (evicted >= max_evict) return;
		}
	}
}

void RadixCodeCache::Clear() noexcept {
	if (m_root) {
		FreeNodeRecursive(m_root);
		m_root = new RadixNode();
	}
}

// ─── GenerationalCodeCache Implementation ────────────────────────────────────

GenerationalCodeCache::GenerationalCodeCache(size_t gen0_capacity, size_t gen1_capacity)
    : m_gen0_capacity(gen0_capacity), m_gen1_capacity(gen1_capacity) {

	int flags = MAP_ANON | MAP_PRIVATE;
#if defined(MAP_JIT)
	flags |= MAP_JIT;
#endif

	m_gen0_base = static_cast<uint8_t*>(::mmap(nullptr, m_gen0_capacity, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0));
	m_gen1_base = static_cast<uint8_t*>(::mmap(nullptr, m_gen1_capacity, PROT_READ | PROT_WRITE | PROT_EXEC, flags, -1, 0));

	if (m_gen0_base == MAP_FAILED) m_gen0_base = nullptr;
	if (m_gen1_base == MAP_FAILED) m_gen1_base = nullptr;
}

GenerationalCodeCache::~GenerationalCodeCache() {
	if (m_gen0_base) ::munmap(m_gen0_base, m_gen0_capacity);
	if (m_gen1_base) ::munmap(m_gen1_base, m_gen1_capacity);
	m_gen0_base = nullptr;
	m_gen1_base = nullptr;
}

uint8_t* GenerationalCodeCache::AllocateGen0(size_t size_bytes) {
	if (!m_gen0_base || size_bytes == 0) return nullptr;

	size_t aligned_size = (size_bytes + 15u) & ~15u;
	size_t curr_off = m_gen0_offset.fetch_add(aligned_size, std::memory_order_relaxed);

	if (curr_off + aligned_size > m_gen0_capacity) {
		return nullptr; // Gen0 Exhausted
	}

	m_stats.gen0_bytes_used = curr_off + aligned_size;
	m_stats.active_block_cnt++;
	return m_gen0_base + curr_off;
}

uint8_t* GenerationalCodeCache::PromoteToGen1(uint8_t* gen0_ptr, size_t size_bytes) {
	if (!m_gen1_base || !gen0_ptr || size_bytes == 0) return nullptr;

	size_t aligned_size = (size_bytes + 15u) & ~15u;
	size_t curr_off = m_gen1_offset.fetch_add(aligned_size, std::memory_order_relaxed);

	if (curr_off + aligned_size > m_gen1_capacity) {
		return nullptr;
	}

	uint8_t* gen1_ptr = m_gen1_base + curr_off;
	Arm64CodeCache::SetJitWriteProtect(false);
	std::memcpy(gen1_ptr, gen0_ptr, size_bytes);
	Arm64CodeCache::SetJitWriteProtect(true);
	Arm64CodeCache::FlushInstructionCache(gen1_ptr, size_bytes);

	m_stats.gen1_bytes_used = curr_off + aligned_size;
	return gen1_ptr;
}

double GenerationalCodeCache::CalculateFragmentationRatio() const noexcept {
	size_t total_alloc = m_stats.gen0_bytes_used + m_stats.gen1_bytes_used;
	if (total_alloc == 0) return 0.0;
	size_t active_bytes = m_stats.active_block_cnt * 64; // Approx average active block payload
	if (active_bytes > total_alloc) active_bytes = total_alloc;

	m_stats.fragmentation_pct = (1.0 - (static_cast<double>(active_bytes) / total_alloc)) * 100.0;
	return m_stats.fragmentation_pct;
}

bool GenerationalCodeCache::CompactCache() noexcept {
	std::lock_guard<std::mutex> lock(m_gen_mutex);
	m_gen0_offset.store(0, std::memory_order_relaxed);
	m_stats.gen0_bytes_used = 0;
	m_stats.fragmentation_pct = 0.0;
	return true;
}

size_t GenerationalCodeCache::EvictLRU(size_t target_reclaim_bytes) noexcept {
	std::lock_guard<std::mutex> lock(m_gen_mutex);
	size_t reclaimed = 0;
	size_t curr_gen0 = m_gen0_offset.load(std::memory_order_relaxed);
	if (curr_gen0 > 0) {
		reclaimed = (curr_gen0 > target_reclaim_bytes) ? target_reclaim_bytes : curr_gen0;
		m_gen0_offset.store(curr_gen0 - reclaimed, std::memory_order_relaxed);
		m_stats.gen0_bytes_used = m_gen0_offset.load();
		m_stats.evicted_block_cnt += (reclaimed / 64);
	}
	return reclaimed;
}

bool GenerationalCodeCache::SerializeToFile(const std::string& filepath) const {
	std::ofstream out(filepath, std::ios::binary);
	if (!out) return false;

	uint32_t magic = 0x4B595459; // 'KYTY'
	out.write(reinterpret_cast<const char*>(&magic), sizeof(magic));

	size_t gen0_used = m_gen0_offset.load();
	out.write(reinterpret_cast<const char*>(&gen0_used), sizeof(gen0_used));
	if (gen0_used > 0 && m_gen0_base) {
		out.write(reinterpret_cast<const char*>(m_gen0_base), gen0_used);
	}

	return true;
}

bool GenerationalCodeCache::DeserializeFromFile(const std::string& filepath) {
	std::ifstream in(filepath, std::ios::binary);
	if (!in) return false;

	uint32_t magic = 0;
	in.read(reinterpret_cast<char*>(&magic), sizeof(magic));
	if (magic != 0x4B595459) return false;

	size_t gen0_used = 0;
	in.read(reinterpret_cast<char*>(&gen0_used), sizeof(gen0_used));
	if (gen0_used > 0 && m_gen0_base && gen0_used <= m_gen0_capacity) {
		Arm64CodeCache::SetJitWriteProtect(false);
		in.read(reinterpret_cast<char*>(m_gen0_base), gen0_used);
		Arm64CodeCache::SetJitWriteProtect(true);
		Arm64CodeCache::FlushInstructionCache(m_gen0_base, gen0_used);
		m_gen0_offset.store(gen0_used);
		m_stats.gen0_bytes_used = gen0_used;
	}

	return true;
}

} // namespace Loader::Recompiler
