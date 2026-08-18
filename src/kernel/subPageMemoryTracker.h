// subPageMemoryTracker.h
//
// 4KB-to-16KB Sub-Page Memory Granularity & Protection Tracking Subsystem for Apple Silicon macOS.

#ifndef KERNEL_SUB_PAGE_MEMORY_TRACKER_H
#define KERNEL_SUB_PAGE_MEMORY_TRACKER_H

#include "common/common.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Kernel {

enum class SubPageProt : uint8_t {
	None  = 0,
	Read  = 1 << 0,
	Write = 1 << 1,
	Exec  = 1 << 2
};

inline SubPageProt operator|(SubPageProt a, SubPageProt b) {
	return static_cast<SubPageProt>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

inline SubPageProt operator&(SubPageProt a, SubPageProt b) {
	return static_cast<SubPageProt>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}

struct Host16KbPageEntry {
	SubPageProt slice_prot[4] = {SubPageProt::None, SubPageProt::None, SubPageProt::None, SubPageProt::None};
	bool        slice_dirty[4] = {false, false, false, false};
	uint8_t     effective_host_prot = 0; // POSIX PROT_NONE, PROT_READ, PROT_WRITE, PROT_EXEC
};

struct SubPageTrackerStats {
	uint64_t total_4kb_pages_tracked = 0;
	uint64_t total_16kb_host_pages = 0;
	uint32_t protection_changes = 0;
	uint32_t sub_page_faults_handled = 0;
	uint32_t split_protection_pages = 0;
};

class SubPageMemoryTracker {
public:
	static constexpr size_t kGuestPageSize = 4096;      // 4 KB
	static constexpr size_t kHostPageSize  = 16384;     // 16 KB
	static constexpr size_t kSlicesPerPage = 4;         // 16KB / 4KB

	SubPageMemoryTracker() = default;
	~SubPageMemoryTracker() = default;

	KYTY_CLASS_NO_COPY(SubPageMemoryTracker);

	// Set guest protection for a 4KB-aligned virtual memory range
	bool SetProtection(uint64_t guest_va, size_t size, SubPageProt prot);

	// Query protection of a specific 4KB guest page
	SubPageProt GetProtection(uint64_t guest_va) const;

	// Check if a specific 4KB guest page is dirty
	bool IsDirty(uint64_t guest_va) const;

	// Mark a 4KB guest slice as dirty
	void MarkDirty(uint64_t guest_va);

	// Clear dirty flags in range
	void ClearDirty(uint64_t guest_va, size_t size);

	// Handle access check: returns true if access is permitted according to 4KB sub-page rules
	bool ValidateAccess(uint64_t guest_va, bool is_write, bool is_exec);

	const SubPageTrackerStats& GetStats() const noexcept { return m_stats; }

	// Calculate host mprotect permissions for a 16KB page
	static uint8_t ComputeHostProtection(const Host16KbPageEntry& entry);

private:
	mutable std::mutex m_mutex;
	std::unordered_map<uint64_t, Host16KbPageEntry> m_host_pages; // Key: 16KB-aligned host base VA
	SubPageTrackerStats m_stats;
};

} // namespace Kernel

#endif // KERNEL_SUB_PAGE_MEMORY_TRACKER_H
