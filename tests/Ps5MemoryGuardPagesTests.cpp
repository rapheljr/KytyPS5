// Ps5MemoryGuardPagesTests.cpp
//
// Unit & Integration Tests for PS5 Virtual Memory Guard Pages & Protection Subsystem.

#include "kernel/ps5MemoryGuardPages.h"

#include <cstdio>
#include <cstdlib>

using namespace Kernel;

static void TestMemoryAllocationAndQuery() {
	std::printf("[TEST] MemoryGuard_AllocationAndQuery\n");

	Ps5MemoryGuardPages mgr;
	uint64_t base = 0x100000000ULL;
	size_t size = 0x200000; // 2MB

	if (!mgr.AllocateRegion(base, size, MemProt_Read | MemProt_Write)) {
		std::fprintf(stderr, "FAIL: AllocateRegion failed\n");
		std::exit(1);
	}

	MemoryRegionInfo query_info{};
	if (!mgr.QueryRegion(base + 0x1000, query_info)) {
		std::fprintf(stderr, "FAIL: QueryRegion failed\n");
		std::exit(1);
	}

	if (query_info.start_address != base || query_info.size != size ||
	    query_info.protection != (MemProt_Read | MemProt_Write)) {
		std::fprintf(stderr, "FAIL: QueryRegion data mismatch\n");
		std::exit(1);
	}

	// Change protection to Read | Exec
	if (!mgr.ProtectRegion(base, size, MemProt_Read | MemProt_Exec)) {
		std::fprintf(stderr, "FAIL: ProtectRegion failed\n");
		std::exit(1);
	}

	mgr.QueryRegion(base, query_info);
	if (query_info.protection != (MemProt_Read | MemProt_Exec)) {
		std::fprintf(stderr, "FAIL: Updated protection mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] MemoryGuard_AllocationAndQuery\n");
}

static void TestGuardPageFaultRecovery() {
	std::printf("[TEST] MemoryGuard_FaultRecovery\n");

	Ps5MemoryGuardPages mgr;
	uint64_t stack_guard = 0x7FFF00000000ULL;
	size_t guard_size = 0x4000; // 16KB

	mgr.AllocateRegion(stack_guard, guard_size, MemProt_Guard | MemProt_Read);

	// Access fault on guard page
	if (!mgr.HandleAccessViolation(stack_guard + 0x100, /*is_write=*/true)) {
		std::fprintf(stderr, "FAIL: HandleAccessViolation did not recover guard page\n");
		std::exit(1);
	}

	MemoryRegionInfo info{};
	mgr.QueryRegion(stack_guard, info);
	if (info.protection & MemProt_Guard) {
		std::fprintf(stderr, "FAIL: Guard flag still set after recovery\n");
		std::exit(1);
	}

	const auto& stats = mgr.GetStats();
	if (stats.guard_faults_handled != 1) {
		std::fprintf(stderr, "FAIL: Guard faults handled stat mismatch: %llu\n", stats.guard_faults_handled);
		std::exit(1);
	}

	std::printf("  [ OK ] MemoryGuard_FaultRecovery\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 Memory Guard Pages & Protection Test Suite\n");
	std::printf("================================================================================\n");

	TestMemoryAllocationAndQuery();
	TestGuardPageFaultRecovery();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
