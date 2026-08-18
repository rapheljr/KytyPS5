// SubPageMemoryTrackerTests.cpp
//
// Unit & Integration tests for 4KB-to-16KB Sub-Page Granularity & Protection Tracking.

#include "kernel/subPageMemoryTracker.h"

#include <cstdio>
#include <cstdlib>
#include <sys/mman.h>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Kernel;

void TestSubPageProtectionSlicing() {
	std::printf("[TEST] 4KB-to-16KB Sub-Page Protection Slicing...\n");

	SubPageMemoryTracker tracker;
	uint64_t base_16k = 0x10000000; // 16KB aligned address

	// Slice 0 (0..4KB): ReadOnly
	// Slice 1 (4..8KB): ReadWrite
	// Slice 2 (8..12KB): ExecOnly
	// Slice 3 (12..16KB): None
	tracker.SetProtection(base_16k + 0x0000, 4096, SubPageProt::Read);
	tracker.SetProtection(base_16k + 0x1000, 4096, SubPageProt::Read | SubPageProt::Write);
	tracker.SetProtection(base_16k + 0x2000, 4096, SubPageProt::Exec);
	tracker.SetProtection(base_16k + 0x3000, 4096, SubPageProt::None);

	Check(tracker.GetProtection(base_16k + 0x0000) == SubPageProt::Read, "Slice 0 prot mismatch");
	Check(tracker.GetProtection(base_16k + 0x1000) == (SubPageProt::Read | SubPageProt::Write), "Slice 1 prot mismatch");
	Check(tracker.GetProtection(base_16k + 0x2000) == SubPageProt::Exec, "Slice 2 prot mismatch");
	Check(tracker.GetProtection(base_16k + 0x3000) == SubPageProt::None, "Slice 3 prot mismatch");

	// Effective host protection check: PROT_READ | PROT_WRITE | PROT_EXEC
	Host16KbPageEntry entry;
	entry.slice_prot[0] = SubPageProt::Read;
	entry.slice_prot[1] = SubPageProt::Read | SubPageProt::Write;
	entry.slice_prot[2] = SubPageProt::Exec;
	entry.slice_prot[3] = SubPageProt::None;

	uint8_t effective = SubPageMemoryTracker::ComputeHostProtection(entry);
	Check((effective & PROT_READ) != 0, "Host page must have PROT_READ");
	Check((effective & PROT_WRITE) != 0, "Host page must have PROT_WRITE");
	Check((effective & PROT_EXEC) != 0, "Host page must have PROT_EXEC");

	// Split page count should be 1
	Check(tracker.GetStats().split_protection_pages == 1, "Split page count should be 1");

	std::printf("  [OK] 4KB-to-16KB Sub-Page Protection Slicing\n");
}

void TestAccessValidationAndFaultHandling() {
	std::printf("[TEST] Sub-Page Access Validation & Fault Tracking...\n");

	SubPageMemoryTracker tracker;
	uint64_t base_16k = 0x20000000;

	tracker.SetProtection(base_16k + 0x0000, 4096, SubPageProt::Read);
	tracker.SetProtection(base_16k + 0x1000, 4096, SubPageProt::Read | SubPageProt::Write);

	// 1. Read on slice 0 (permitted)
	Check(tracker.ValidateAccess(base_16k + 0x0100, false, false), "Read on slice 0 should pass");

	// 2. Write on slice 0 (must fail and record fault)
	Check(!tracker.ValidateAccess(base_16k + 0x0100, true, false), "Write on read-only slice 0 should fail");
	Check(tracker.GetStats().sub_page_faults_handled == 1, "Expected 1 fault recorded");

	// 3. Write on slice 1 (permitted and marks slice dirty)
	Check(tracker.ValidateAccess(base_16k + 0x1100, true, false), "Write on slice 1 should pass");
	Check(tracker.IsDirty(base_16k + 0x1100), "Slice 1 should be dirty after write");
	Check(!tracker.IsDirty(base_16k + 0x0100), "Slice 0 should not be dirty");

	// 4. Clear dirty
	tracker.ClearDirty(base_16k, 16384);
	Check(!tracker.IsDirty(base_16k + 0x1100), "Slice 1 dirty flag should be cleared");

	std::printf("  [OK] Sub-Page Access Validation & Fault Tracking\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — 4KB-to-16KB Sub-Page Memory Translation Test Suite\n");
	std::printf("================================================================================\n");

	TestSubPageProtectionSlicing();
	TestAccessValidationAndFaultHandling();

	std::printf("================================================================================\n");
	std::printf("  Results: All Sub-Page Memory Translation Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
