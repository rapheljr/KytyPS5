// ps5MemoryGuardPages.h
//
// PS5 Virtual Memory Guard Pages & User-Space Protection Engine for KytyPS5.
// Emulates sceKernelVirtualQuery, sceKernelProtect, and guard page faults.

#ifndef KERNEL_PS5_MEMORY_GUARD_PAGES_H
#define KERNEL_PS5_MEMORY_GUARD_PAGES_H

#include "common/common.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <vector>

namespace Kernel {

enum MemoryProtection : uint32_t {
	MemProt_None  = 0,
	MemProt_Read  = (1 << 0),
	MemProt_Write = (1 << 1),
	MemProt_Exec  = (1 << 2),
	MemProt_Guard = (1 << 3)
};

struct MemoryRegionInfo {
	uint64_t start_address = 0;
	size_t   size          = 0;
	uint32_t protection    = MemProt_None;
	bool     is_committed  = false;
};

struct MemoryGuardStats {
	uint64_t total_allocations      = 0;
	uint64_t protection_changes     = 0;
	uint64_t guard_faults_handled   = 0;
	uint64_t active_regions         = 0;
};

class Ps5MemoryGuardPages {
public:
	Ps5MemoryGuardPages();
	~Ps5MemoryGuardPages() = default;

	KYTY_CLASS_NO_COPY(Ps5MemoryGuardPages);

	/// Allocate virtual memory address space with protection
	bool AllocateRegion(uint64_t start_addr, size_t size, uint32_t protection);

	/// Change memory protection (sceKernelProtect)
	bool ProtectRegion(uint64_t start_addr, size_t size, uint32_t new_protection);

	/// Query memory region (sceKernelVirtualQuery)
	bool QueryRegion(uint64_t address, MemoryRegionInfo& out_info);

	/// Free virtual memory region
	bool FreeRegion(uint64_t start_addr);

	/// Handle memory access fault (returns true if recovered e.g. expanded guard page)
	bool HandleAccessViolation(uint64_t fault_addr, bool is_write);

	[[nodiscard]] const MemoryGuardStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	std::map<uint64_t, MemoryRegionInfo> m_regions;
	std::mutex                           m_mutex;
	MemoryGuardStats                     m_stats{};
};

} // namespace Kernel

#endif // KERNEL_PS5_MEMORY_GUARD_PAGES_H
