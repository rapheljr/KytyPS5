// x86RecompilerIR.cpp
//
// Intermediate Representation, Basic Block & CFG for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86RecompilerIR.h"

namespace Loader::Recompiler {

std::unique_ptr<RecompilerBasicBlock> X86BlockBuilder::BuildBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t start_rip) {
	auto block = std::make_unique<RecompilerBasicBlock>(start_rip);

	if (!code_ptr || max_bytes == 0) {
		return block;
	}

	size_t offset = 0;
	uint64_t current_rip = start_rip;

	constexpr size_t kMaxBlockInstructions = 100;
	size_t inst_count = 0;

	while (offset < max_bytes && inst_count < kMaxBlockInstructions) {
		DecodedX86Instruction decoded = X86Decoder::DecodeInstruction(code_ptr + offset, max_bytes - offset, current_rip);

		if (decoded.length == 0 || decoded.opcode == X86Opcode::Invalid) {
			break;
		}

		RecompilerIRInstruction ir_inst{};
		ir_inst.opcode      = decoded.opcode;
		ir_inst.cond        = decoded.cond;
		ir_inst.cond_invert = decoded.cond_invert;
		ir_inst.dst         = decoded.dst;
		ir_inst.src         = decoded.src;
		ir_inst.guest_rip   = current_rip;
		ir_inst.active      = true;

		block->AddInstruction(ir_inst);

		offset += decoded.length;
		current_rip += decoded.length;
		inst_count++;

		// Terminal block boundaries: RET, JMP, or JCC
		if (decoded.opcode == X86Opcode::Ret || decoded.opcode == X86Opcode::Jmp || decoded.opcode == X86Opcode::Jcc) {
			break;
		}
	}

	block->SetEndRip(current_rip);
	OptimizeBlock(*block);

	return block;
}

bool X86BlockBuilder::OptimizeBlock(RecompilerBasicBlock& block) {
	bool modified = false;
	auto& insts = block.GetInstructions();

	// Dead NOP removal & redundant MOV folding
	for (auto& inst : insts) {
		if (!inst.active) continue;

		if (inst.opcode == X86Opcode::Nop) {
			inst.active = false;
			modified = true;
		} else if (inst.opcode == X86Opcode::Mov && inst.dst.kind == X86Operand::Kind::Reg && inst.src.kind == X86Operand::Kind::Reg) {
			if (inst.dst.reg == inst.src.reg) {
				inst.active = false; // Fold mov rax, rax
				modified = true;
			}
		}
	}

	return modified;
}

} // namespace Loader::Recompiler
