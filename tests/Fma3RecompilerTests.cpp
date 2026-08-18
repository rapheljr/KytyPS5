// Fma3RecompilerTests.cpp
//
// Unit & Integration tests for x86-64 VEX FMA3 Instructions and IR Lowering.

#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/x86Decoder.h"
#include "loader/recompiler/x86ToIRLowering.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestFma3Decoding() {
	std::printf("[TEST] VEX FMA3 Instruction Decoding...\n");

	const uint8_t vfmadd132ps_code[] = {0xC4, 0xE2, 0x69, 0x98, 0xCB};
	auto dec1 = X86Decoder::DecodeInstruction(vfmadd132ps_code, sizeof(vfmadd132ps_code), 0x1000);
	std::printf("  Decoded: opcode=%d, dst.reg=%d, src.reg=%d, vex_vvvv=%d\n",
	            (int)dec1.opcode, (int)dec1.dst.reg, (int)dec1.src.reg, (int)dec1.vex_vvvv);
	Check(dec1.opcode == X86Opcode::Vfmadd132ps, "Expected Vfmadd132ps opcode");
	Check(dec1.dst.kind == X86Operand::Kind::Reg, "Dst kind mismatch");
	Check(dec1.src.kind == X86Operand::Kind::Reg, "Src kind mismatch");
	Check(dec1.vex_vvvv == 2 /* xmm2 */, "VEX vvvv mismatch");

	// 2. vfmadd213ps xmm4, xmm5, xmm6
	// 0xC4, 0xE2, 0x51 (vvvv=5 => xmm5), 0xA8, 0xE6 (reg=4, rm=6)
	const uint8_t vfmadd213ps_code[] = {0xC4, 0xE2, 0x51, 0xA8, 0xE6};
	auto dec2 = X86Decoder::DecodeInstruction(vfmadd213ps_code, sizeof(vfmadd213ps_code), 0x1005);
	Check(dec2.opcode == X86Opcode::Vfmadd213ps, "Expected Vfmadd213ps opcode");

	// 3. vfmadd231ps xmm0, xmm1, xmm2
	// 0xC4, 0xE2, 0x71 (vvvv=1 => xmm1), 0xB8, 0xC2 (reg=0, rm=2)
	const uint8_t vfmadd231ps_code[] = {0xC4, 0xE2, 0x71, 0xB8, 0xC2};
	auto dec3 = X86Decoder::DecodeInstruction(vfmadd231ps_code, sizeof(vfmadd231ps_code), 0x100A);
	Check(dec3.opcode == X86Opcode::Vfmadd231ps, "Expected Vfmadd231ps opcode");

	// 4. vfmsub132ps xmm7, xmm8, xmm9
	const uint8_t vfmsub132ps_code[] = {0xC4, 0x62, 0x39, 0x9E, 0xF9};
	auto dec4 = X86Decoder::DecodeInstruction(vfmsub132ps_code, sizeof(vfmsub132ps_code), 0x1010);
	Check(dec4.opcode == X86Opcode::Vfmsub132ps, "Expected Vfmsub132ps opcode");

	std::printf("  [OK] VEX FMA3 Instruction Decoding\n");
}

void TestFma3IRLowering() {
	std::printf("[TEST] VEX FMA3 to Compiler IR Lowering...\n");

	const uint8_t fma_block[] = {
		0xC4, 0xE2, 0x69, 0x98, 0xCB, // vfmadd132ps xmm1, xmm2, xmm3
		0xC4, 0x62, 0x39, 0x9E, 0xF9, // vfmsub132ps xmm7, xmm8, xmm9
		0xC3                          // ret
	};

	auto cfg = X86ToIRLowering::LowerBlock(fma_block, sizeof(fma_block), 0x2000);
	Check(cfg != nullptr, "CFG generation failed");
	Check(cfg->GetBlocks().size() == 1, "Expected single basic block");

	const auto& insts = cfg->GetBlocks()[0]->GetInstructions();
	Check(insts.size() >= 3, "Expected at least 3 IR instructions");
	Check(insts[0]->GetOpcode() == IROpcode::VecFmadd, "Expected VecFmadd IR opcode");
	Check(insts[0]->GetOperands().size() == 3, "VecFmadd must have 3 operands");
	Check(insts[1]->GetOpcode() == IROpcode::VecFmsub, "Expected VecFmsub IR opcode");
	Check(insts[2]->GetOpcode() == IROpcode::Return, "Expected Return IR opcode");

	std::printf("  [OK] VEX FMA3 to Compiler IR Lowering\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — FMA3 JIT Vector Math & IR Lowering Test Suite\n");
	std::printf("================================================================================\n");

	TestFma3Decoding();
	TestFma3IRLowering();

	std::printf("================================================================================\n");
	std::printf("  Results: All FMA3 Recompiler Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
