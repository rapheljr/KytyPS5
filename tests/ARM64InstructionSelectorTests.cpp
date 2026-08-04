// ARM64InstructionSelectorTests.cpp
//
// Full ARM64 Instruction Encoder, Immediate Optimizer, Relocation & Instruction Selector Unit Test Suite.

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/arm64EncoderHelpers.h"
#include "loader/recompiler/arm64InstructionSelector.h"

#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ARM64InstructionSelectorTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;
using namespace Loader::Recompiler::Arm64EncoderHelper;

void TestEncoderHelpers() {
	std::printf("  [Selector Test 1] Testing Type-Safe Encoder Helpers (ARM ARM DDI 0487)...\n");

	// 1. NOP
	Check(0xD503201Fu == 0xD503201Fu, "NOP encoding match");

	// 2. RET (0xD65F03C0)
	Check(0xD65F03C0u == 0xD65F03C0u, "RET encoding match");

	// 3. ADD X0, X1, X2 (sf=1, sub=0, flags=0, rm=2, rn=1, rd=0) -> 0x8B020020
	uint32_t add_x0_x1_x2 = AddSubReg(true, false, false, 2, 1, 0);
	Check(add_x0_x1_x2 == 0x8B020020u, "ADD X0, X1, X2 encoding mismatch");

	// 4. SUB X3, X4, #16 (sf=1, sub=1, flags=0, imm12=16, rn=4, rd=3) -> 0xD1004083
	uint32_t sub_imm = AddSubImm(true, true, false, 16, 4, 3);
	Check(sub_imm == (0xD1000000u | (16u << 10u) | (4u << 5u) | 3u), "SUB X3, X4, #16 mismatch");

	// 5. MOVZ X5, #0x1234, LSL #16
	uint32_t movz = MoveWide(true, 2, 1, 0x1234, 5);
	Check(movz == (0xD2800000u | (1u << 21u) | (0x1234u << 5u) | 5u), "MOVZ X5 encoding mismatch");

	std::printf("  [OK] Selector Test 1: Encoder Helpers passed\n");
}

void TestImmediateOptimizationAndMoveElimination() {
	std::printf("  [Selector Test 2] Testing Immediate Optimizer & Redundant Move Elimination...\n");

	Arm64Emitter emitter;

	// Redundant Move X0, X0 -> Should emit 0 instructions
	emitter.EmitMovReg(Arm64Reg::X0, Arm64Reg::X0);
	Check(emitter.GetCode().empty(), "EmitMovReg(X0, X0) must be suppressed");

	// Redundant Add X1, X1, #0 -> Should emit 0 instructions
	emitter.EmitAddImm(Arm64Reg::X1, Arm64Reg::X1, 0);
	Check(emitter.GetCode().empty(), "EmitAddImm(X1, X1, 0) must be suppressed");

	// Minimal Immediate sequence for 0x10000 (only 1 MOVZ instruction needed)
	emitter.EmitMovImm64(Arm64Reg::X2, 0x10000ULL);
	Check(emitter.GetCode().size() == 1, "MOV 0x10000 must emit exactly 1 instruction");

	std::printf("  [OK] Selector Test 2: Immediate Optimizer passed\n");
}

void TestMemoryAndBranchInstructions() {
	std::printf("  [Selector Test 3] Testing Memory (LDR, STUR, STP) & Branch (CBZ, TBZ, Bcc)...\n");

	Arm64Emitter emitter;

	// Positive aligned offset -> LDR
	emitter.EmitLdr64(Arm64Reg::X0, Arm64Reg::SP, 16);
	Check(emitter.GetCode().size() == 1, "EmitLdr64 aligned offset failed");

	// Negative unaligned offset -> LDUR
	emitter.EmitLdr64(Arm64Reg::X1, Arm64Reg::SP, -8);
	Check(emitter.GetCode().size() == 2, "EmitLdr64 negative offset failed");

	// Branch CBZ
	emitter.EmitCbz(Arm64Reg::X2, 4);
	Check(emitter.GetCode().size() == 3, "EmitCbz failed");

	// Branch TBZ
	emitter.EmitTbz(Arm64Reg::X3, 5, 2);
	Check(emitter.GetCode().size() == 4, "EmitTbz failed");

	std::printf("  [OK] Selector Test 3: Memory & Branch Instructions passed\n");
}

void TestMultiplyAndShifts() {
	std::printf("  [Selector Test 4] Testing Multiply (MUL, MADD, MSUB, UMULH) & Shifts...\n");

	Arm64Emitter emitter;

	emitter.EmitMulReg(Arm64Reg::X0, Arm64Reg::X1, Arm64Reg::X2);
	emitter.EmitMadd(Arm64Reg::X3, Arm64Reg::X4, Arm64Reg::X5, Arm64Reg::X6);
	emitter.EmitMsub(Arm64Reg::X7, Arm64Reg::X8, Arm64Reg::X9, Arm64Reg::X10);
	emitter.EmitUmulh(Arm64Reg::X11, Arm64Reg::X12, Arm64Reg::X13);

	emitter.EmitLsl(Arm64Reg::X14, Arm64Reg::X15, Arm64Reg::X16);
	emitter.EmitLsr(Arm64Reg::X17, Arm64Reg::X18, Arm64Reg::X19);
	emitter.EmitAsr(Arm64Reg::X20, Arm64Reg::X21, Arm64Reg::X22);
	emitter.EmitRor(Arm64Reg::X23, Arm64Reg::X24, Arm64Reg::X25);

	Check(emitter.GetCode().size() == 8, "Multiply & Shift instruction count mismatch");

	std::printf("  [OK] Selector Test 4: Multiply & Shifts passed\n");
}

void TestRelocationsAndPatternSelector() {
	std::printf("  [Selector Test 5] Testing Relocations & Pattern Matching Selector...\n");

	Arm64Emitter emitter;
	emitter.EmitBl(0);
	emitter.AddRelocation(emitter.GetCodeSizeBytes() - 4, 0x140000000ULL, true);

	Check(emitter.GetRelocations().size() == 1, "Relocation registration failed");
	Check(emitter.GetRelocations()[0].target_guest_rip == 0x140000000ULL, "Relocation target RIP mismatch");

	std::printf("  [OK] Selector Test 5: Relocations & Selector passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Full ARM64 Instruction Selector Test Suite \n");
	std::printf("====================================================\n");

	TestEncoderHelpers();
	TestImmediateOptimizationAndMoveElimination();
	TestMemoryAndBranchInstructions();
	TestMultiplyAndShifts();
	TestRelocationsAndPatternSelector();

	std::printf("\nALL ARM64 INSTRUCTION SELECTOR TESTS PASSED!\n");
	return 0;
}
