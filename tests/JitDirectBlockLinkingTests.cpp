// JitDirectBlockLinkingTests.cpp
//
// Unit & Integration Tests for JIT Direct Host Basic Block Linking & Dynamic Edge Invalidation.

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/x86BlockCache.h"
#include "loader/recompiler/jitBlockLinker.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestBranchEncoding() {
	std::printf("[TEST] JitBlockLinker_BranchEncoding\n");

	// Test Unconditional B with positive and negative offsets
	uint32_t b_fwd = JitBlockLinker::EncodeBranch(16);
	// 16 >> 2 = 4 -> 0x14000004
	Check(b_fwd == 0x14000004, "EncodeBranch forward offset mismatch");

	uint32_t b_bwd = JitBlockLinker::EncodeBranch(-16);
	// -16 >> 2 = -4 -> 0x14000000 | (0xFFFFFFFC & 0x03FFFFFF) = 0x17FFFFFC
	Check(b_bwd == 0x17FFFFFC, "EncodeBranch backward offset mismatch");

	// Test Conditional B.cond (EQ, NE, GT, LT)
	uint32_t b_eq = JitBlockLinker::EncodeConditionalBranch(32, BranchCondition::EQ);
	// 32 >> 2 = 8, (8 << 5) | 0 = 0x100 -> 0x54000100
	Check(b_eq == 0x54000100, "EncodeConditionalBranch EQ mismatch");

	uint32_t b_ne = JitBlockLinker::EncodeConditionalBranch(32, BranchCondition::NE);
	// (8 << 5) | 1 = 0x101 -> 0x54000101
	Check(b_ne == 0x54000101, "EncodeConditionalBranch NE mismatch");

	std::printf("  [ OK ] JitBlockLinker_BranchEncoding\n");
}

void TestDirectBlockLinkingAndUnlinking() {
	std::printf("[TEST] JitBlockLinker_DirectBlockLinkingAndUnlinking\n");

	Arm64CodeCache cache(64 * 1024);
	JitBlockLinker linker;

	// Allocate 3 mock blocks in JIT memory
	// Block A (at 0x401000): ends with a branch site that wants to go to Block B (0x402000)
	uint8_t* block_a = cache.AllocateCode(64);
	uint8_t* block_b = cache.AllocateCode(64);
	uint8_t* stub_dispatcher = cache.AllocateCode(64);

	Check(block_a != nullptr && block_b != nullptr && stub_dispatcher != nullptr, "JIT allocations failed");

	// Initialize block A with a placeholder NOP (0xD503201F) at branch site
	uint8_t* branch_site = block_a + 32;
	uint32_t nop = 0xD503201F;
	Arm64CodeCache::SetJitWriteProtect(false);
	std::memcpy(branch_site, &nop, sizeof(nop));
	Arm64CodeCache::SetJitWriteProtect(true);
	Arm64CodeCache::FlushInstructionCache(branch_site, sizeof(nop));

	BlockLinkEdge edge_a{};
	edge_a.branch_site_host_addr = branch_site;
	edge_a.target_guest_rip       = 0x402000;
	edge_a.is_conditional         = false;
	edge_a.fallback_exit_stub     = stub_dispatcher;

	// Register Block A before Block B is compiled
	linker.RegisterBlock(0x401000, block_a, { edge_a });
	Check(linker.GetRegisteredBlockCount() == 1, "Expected 1 registered block");
	Check(linker.GetActiveLinkCount() == 0, "No active links should exist before target is registered");

	// Now register Block B (0x402000)
	linker.RegisterBlock(0x402000, block_b, {});
	Check(linker.GetRegisteredBlockCount() == 2, "Expected 2 registered blocks");
	Check(linker.GetActiveLinkCount() == 1, "Edge from A to B should be linked automatically");

	// Verify that branch_site now contains a branch instruction targeting block_b
	uint32_t patched_inst = 0;
	std::memcpy(&patched_inst, branch_site, sizeof(patched_inst));
	int64_t expected_offset = reinterpret_cast<intptr_t>(block_b) - reinterpret_cast<intptr_t>(branch_site);
	uint32_t expected_b = JitBlockLinker::EncodeBranch(expected_offset);
	Check(patched_inst == expected_b, "Patched instruction does not match target branch");

	// Test Unlinking (e.g. self-modifying code at Block B)
	linker.UnlinkBlock(0x402000);
	Check(linker.GetActiveLinkCount() == 0, "Active links should be 0 after unlinking Block B");

	// Verify that branch_site has been reverted to targeting stub_dispatcher
	std::memcpy(&patched_inst, branch_site, sizeof(patched_inst));
	int64_t stub_offset = reinterpret_cast<intptr_t>(stub_dispatcher) - reinterpret_cast<intptr_t>(branch_site);
	uint32_t expected_stub_b = JitBlockLinker::EncodeBranch(stub_offset);
	Check(patched_inst == expected_stub_b, "Branch was not reverted to fallback dispatcher stub");

	std::printf("  [ OK ] JitBlockLinker_DirectBlockLinkingAndUnlinking\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — JIT Direct Host Block Linking & Edge Invalidation Test Suite\n");
	std::printf("================================================================================\n");

	TestBranchEncoding();
	TestDirectBlockLinkingAndUnlinking();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
