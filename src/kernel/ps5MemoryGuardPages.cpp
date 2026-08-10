// ps5MemoryGuardPages.cpp
//
// PS5 Virtual Memory Guard Pages & Protection Implementation.

#include "kernel/ps5MemoryGuardPages.h"

namespace Kernel {

Ps5MemoryGuardPages::Ps5MemoryGuardPages() = default;

bool Ps5MemoryGuardPages::AllocateRegion(uint64_t start_addr, size_t size, uint32_t protection) {
	if (size == 0) return false;

	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t end_addr = start_addr + size;

	// Check collision
	for (const auto& [addr, info] : m_regions) {
		uint64_t reg_end = addr + info.size;
		if (start_addr < reg_end && end_addr > addr) {
			return false; // Overlap
		}
	}

	MemoryRegionInfo info;
	info.start_address = start_addr;
	info.size          = size;
	info.protection    = protection;
	info.is_committed  = true;

	m_regions[start_addr] = info;
	m_stats.total_allocations++;
	m_stats.active_regions = m_regions.size();

	return true;
}

bool Ps5MemoryGuardPages::ProtectRegion(uint64_t start_addr, size_t size, uint32_t new_protection) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_regions.find(start_addr);
	if (it == m_regions.end()) return false;

	it->second.protection = new_protection;
	m_stats.protection_changes++;
	return true;
}

bool Ps5MemoryGuardPages::QueryRegion(uint64_t address, MemoryRegionInfo& out_info) {
	std::lock_guard<std::mutex> lock(m_mutex);

	for (const auto& [addr, info] : m_regions) {
		if (address >= addr && address < addr + info.size) {
			out_info = info;
			return true;
		}
	}

	return false;
}

bool Ps5MemoryGuardPages::FreeRegion(uint64_t start_addr) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_regions.find(start_addr);
	if (it != m_regions.end()) {
		m_regions.erase(it);
		m_stats.active_regions = m_regions.size();
		return true;
	}

	return false;
}

bool Ps5MemoryGuardPages::HandleAccessViolation(uint64_t fault_addr, bool is_write) {
	std::lock_guard<std::mutex> lock(m_mutex);

	for (auto& [addr, info] : m_regions) {
		if (fault_addr >= addr && fault_addr < addr + info.size) {
			if (info.protection & MemProt_Guard) {
				// Guard page hit -> remove guard and commit as RW (stack expansion)
				info.protection &= ~MemProt_Guard;
				info.protection |= (MemProt_Read | MemProt_Write);
				m_stats.guard_faults_handled++;
				return true; // Successfully recovered
			}
			break;
		}
	}

	return false;
}

void Ps5MemoryGuardPages::Reset() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_regions.clear();
	m_stats = {};
}

} // namespace Kernel
