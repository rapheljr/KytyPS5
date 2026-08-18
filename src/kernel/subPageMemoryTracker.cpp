// subPageMemoryTracker.cpp
//
// 4KB-to-16KB Sub-Page Memory Granularity & Protection Tracking Implementation.

#include "kernel/subPageMemoryTracker.h"

#include <sys/mman.h>

namespace Kernel {

uint8_t SubPageMemoryTracker::ComputeHostProtection(const Host16KbPageEntry& entry) {
	uint8_t host_prot = PROT_NONE;
	for (int i = 0; i < 4; ++i) {
		uint8_t p = static_cast<uint8_t>(entry.slice_prot[i]);
		if (p & static_cast<uint8_t>(SubPageProt::Read))  host_prot |= PROT_READ;
		if (p & static_cast<uint8_t>(SubPageProt::Write)) host_prot |= (PROT_READ | PROT_WRITE);
		if (p & static_cast<uint8_t>(SubPageProt::Exec))  host_prot |= PROT_EXEC;
	}
	return host_prot;
}

bool SubPageMemoryTracker::SetProtection(uint64_t guest_va, size_t size, SubPageProt prot) {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t start_va = guest_va & ~(kGuestPageSize - 1);
	uint64_t end_va   = (guest_va + size + kGuestPageSize - 1) & ~(kGuestPageSize - 1);

	for (uint64_t va = start_va; va < end_va; va += kGuestPageSize) {
		uint64_t host_base_va = va & ~(kHostPageSize - 1);
		uint32_t slice_idx = static_cast<uint32_t>((va - host_base_va) / kGuestPageSize);

		auto& page_entry = m_host_pages[host_base_va];
		page_entry.slice_prot[slice_idx] = prot;

		uint8_t new_host_prot = ComputeHostProtection(page_entry);
		page_entry.effective_host_prot = new_host_prot;

		m_stats.total_4kb_pages_tracked++;
	}

	m_stats.total_16kb_host_pages = m_host_pages.size();
	m_stats.protection_changes++;

	// Update split protection page count
	uint32_t split_count = 0;
	for (const auto& [_, entry] : m_host_pages) {
		SubPageProt first_p = entry.slice_prot[0];
		bool is_split = false;
		for (int i = 1; i < 4; ++i) {
			if (entry.slice_prot[i] != first_p) {
				is_split = true;
				break;
			}
		}
		if (is_split) split_count++;
	}
	m_stats.split_protection_pages = split_count;

	return true;
}

SubPageProt SubPageMemoryTracker::GetProtection(uint64_t guest_va) const {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t host_base_va = guest_va & ~(kHostPageSize - 1);
	uint32_t slice_idx = static_cast<uint32_t>((guest_va - host_base_va) / kGuestPageSize);

	auto it = m_host_pages.find(host_base_va);
	if (it == m_host_pages.end()) return SubPageProt::None;

	return it->second.slice_prot[slice_idx];
}

bool SubPageMemoryTracker::IsDirty(uint64_t guest_va) const {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t host_base_va = guest_va & ~(kHostPageSize - 1);
	uint32_t slice_idx = static_cast<uint32_t>((guest_va - host_base_va) / kGuestPageSize);

	auto it = m_host_pages.find(host_base_va);
	if (it == m_host_pages.end()) return false;

	return it->second.slice_dirty[slice_idx];
}

void SubPageMemoryTracker::MarkDirty(uint64_t guest_va) {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t host_base_va = guest_va & ~(kHostPageSize - 1);
	uint32_t slice_idx = static_cast<uint32_t>((guest_va - host_base_va) / kGuestPageSize);

	auto it = m_host_pages.find(host_base_va);
	if (it != m_host_pages.end()) {
		it->second.slice_dirty[slice_idx] = true;
	}
}

void SubPageMemoryTracker::ClearDirty(uint64_t guest_va, size_t size) {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t start_va = guest_va & ~(kGuestPageSize - 1);
	uint64_t end_va   = (guest_va + size + kGuestPageSize - 1) & ~(kGuestPageSize - 1);

	for (uint64_t va = start_va; va < end_va; va += kGuestPageSize) {
		uint64_t host_base_va = va & ~(kHostPageSize - 1);
		uint32_t slice_idx = static_cast<uint32_t>((va - host_base_va) / kGuestPageSize);

		auto it = m_host_pages.find(host_base_va);
		if (it != m_host_pages.end()) {
			it->second.slice_dirty[slice_idx] = false;
		}
	}
}

bool SubPageMemoryTracker::ValidateAccess(uint64_t guest_va, bool is_write, bool is_exec) {
	std::lock_guard<std::mutex> lock(m_mutex);

	uint64_t host_base_va = guest_va & ~(kHostPageSize - 1);
	uint32_t slice_idx = static_cast<uint32_t>((guest_va - host_base_va) / kGuestPageSize);

	auto it = m_host_pages.find(host_base_va);
	if (it == m_host_pages.end()) {
		m_stats.sub_page_faults_handled++;
		return false;
	}

	SubPageProt p = it->second.slice_prot[slice_idx];
	if (p == SubPageProt::None) {
		m_stats.sub_page_faults_handled++;
		return false;
	}

	if (is_write) {
		if ((static_cast<uint8_t>(p) & static_cast<uint8_t>(SubPageProt::Write)) == 0) {
			m_stats.sub_page_faults_handled++;
			return false;
		}
		it->second.slice_dirty[slice_idx] = true;
	}

	if (is_exec && (static_cast<uint8_t>(p) & static_cast<uint8_t>(SubPageProt::Exec)) == 0) {
		m_stats.sub_page_faults_handled++;
		return false;
	}

	return true;
}

} // namespace Kernel
