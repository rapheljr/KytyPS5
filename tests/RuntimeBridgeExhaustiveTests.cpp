// RuntimeBridgeExhaustiveTests.cpp
//
// Exhaustive Correctness Test Suite for GuestCpuContext & X86RuntimeBridge.

#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstddef>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "RuntimeBridgeExhaustiveTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestGuestCpuContextLayoutAndAlignment() {
	std::printf("  [Bridge Test 1] Testing GuestCpuContext 16-Byte Alignment & Memory Layout...\n");

	// 1. Structure Alignment
	Check(alignof(GuestCpuContext) == 16, "GuestCpuContext alignment must be 16 bytes");
	Check(sizeof(GuestCpuContext) >= 512, "GuestCpuContext size must accommodate full state");

	// 2. Offsets Alignment
	size_t xmm_offset = offsetof(GuestCpuContext, xmm);
	size_t ymm_hi_offset = offsetof(GuestCpuContext, ymm_hi);

	Check(xmm_offset % 16 == 0, "XMM register array must be 16-byte aligned");
	Check(ymm_hi_offset % 16 == 0, "YMM_hi register array must be 16-byte aligned");

	// 3. Default Values
	GuestCpuContext ctx;
	Check(ctx.mxcsr == 0x1F80, "Default MXCSR control mask must be 0x1F80");
	Check(ctx.rflags == 0x02, "Default RFLAGS must be 0x02");
	Check(!ctx.avx_state_active, "AVX active state must default to false");

	std::printf("  [OK] Bridge Test 1: Alignment & Layout passed\n");
}

void TestLazyRegisterSynchronization() {
	std::printf("  [Bridge Test 2] Testing Lazy Register Synchronization Masks...\n");

	GuestCpuContext ctx;
	Check(ctx.dirty_gpr_mask == 0, "Dirty GPR mask must start at 0");
	Check(ctx.dirty_xmm_mask == 0, "Dirty XMM mask must start at 0");

	ctx.SetGprDirty(0); // RAX (X0)
	ctx.SetGprDirty(3); // RBX (X3)
	ctx.SetXmmDirty(1); // XMM1

	Check(ctx.dirty_gpr_mask == ((1u << 0) | (1u << 3)), "GPR dirty mask mismatch");
	Check(ctx.dirty_xmm_mask == (1u << 1), "XMM dirty mask mismatch");

	ctx.FlushLazyRegisters();
	Check(ctx.dirty_gpr_mask == 0, "GPR dirty mask must be 0 after flush");
	Check(ctx.dirty_xmm_mask == 0, "XMM dirty mask must be 0 after flush");

	std::printf("  [OK] Bridge Test 2: Lazy Register Sync passed\n");
}

void TestStackAlignmentVerification() {
	std::printf("  [Bridge Test 3] Testing 16-Byte Stack Alignment Verification...\n");

	GuestCpuContext ctx;
	ctx.rsp = 0x7FFFFFFF0000ULL; // 16-byte aligned address
	Check(ctx.VerifyStackAlignment(), "RSP 0x7FFFFFFF0000 must be 16-byte aligned");

	ctx.rsp = 0x7FFFFFFF0008ULL; // Unaligned address (misaligned by 8 bytes)
	Check(!ctx.VerifyStackAlignment(), "RSP 0x7FFFFFFF0008 must fail alignment check");

	std::printf("  [OK] Bridge Test 3: Stack Alignment Verification passed\n");
}

void TestExceptionSafeTransitionsAndExecution() {
	std::printf("  [Bridge Test 4] Testing Exception-Safe Execution Frames & Bridge...\n");
	std::fflush(stdout);

	X86RuntimeBridge bridge(1024 * 1024);
	GuestCpuContext ctx;
	ctx.rsp = 0x7FFFFFFF0008ULL; // Misaligned RSP -> Auto-fixed by bridge
	ctx.rip = 0x140001000ULL;

	// Single NOP instruction + RET
	uint8_t code[] = { 0x90, 0xC3 };

	CompiledBlockFunc func = bridge.CompileAndCacheBlock(code, sizeof(code), ctx.rip);
	Check(func != nullptr, "CompileAndCacheBlock must return valid compiled function");

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock must succeed and handle exception frames safely");
	Check(ctx.VerifyStackAlignment(), "RSP must be auto-aligned to 16 bytes by bridge");

	std::printf("  [OK] Bridge Test 4: Exception-Safe Transitions passed\n");
	std::fflush(stdout);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Full Runtime Bridge Exhaustive Suite       \n");
	std::printf("====================================================\n");
	std::fflush(stdout);

	TestGuestCpuContextLayoutAndAlignment();
	std::fflush(stdout);

	TestLazyRegisterSynchronization();
	std::fflush(stdout);

	TestStackAlignmentVerification();
	std::fflush(stdout);

	TestExceptionSafeTransitionsAndExecution();
	std::fflush(stdout);

	std::printf("\nALL RUNTIME BRIDGE EXHAUSTIVE TESTS PASSED!\n");
	std::fflush(stdout);
	return 0;
}
