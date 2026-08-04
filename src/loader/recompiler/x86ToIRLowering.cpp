// x86ToIRLowering.cpp
//
// Front-End x86-64 to Target-Independent Compiler IR Lowering Engine.

#include "loader/recompiler/x86ToIRLowering.h"

namespace Loader::Recompiler {

Value X86ToIRLowering::MapX86OperandToIR(ControlFlowGraph& cfg, BasicBlock* bb, const X86Operand& op) {
	if (op.kind == X86Operand::Kind::Reg) {
		VirtualReg vr = cfg.AllocateVReg(op.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
		vr.phys_pin = static_cast<int8_t>(op.reg);
		return Value::MakeVReg(vr);
	} else if (op.kind == X86Operand::Kind::Imm) {
		return Value::MakeImmInt(op.imm, op.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
	} else if (op.kind == X86Operand::Kind::Mem) {
		VirtualReg base{};
		VirtualReg index{};
		base.phys_pin = static_cast<int8_t>(op.base_reg);
		index.phys_pin = static_cast<int8_t>(op.index_reg);
		return Value::MakeMemory(base, index, op.scale, op.disp);
	}
	return Value::MakeImmInt(0);
}

std::unique_ptr<ControlFlowGraph> X86ToIRLowering::LowerBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t start_rip) {
	auto cfg = std::make_unique<ControlFlowGraph>();
	BasicBlock* entry_bb = cfg->CreateBlock("entry_0x" + std::to_string(start_rip));
	cfg->SetEntryBlock(entry_bb);

	if (!code_ptr || max_bytes == 0) return cfg;

	size_t offset = 0;
	uint64_t current_rip = start_rip;
	constexpr size_t kMaxInstructions = 100;
	size_t count = 0;

	while (offset < max_bytes && count < kMaxInstructions) {
		DecodedX86Instruction decoded = X86Decoder::DecodeInstruction(code_ptr + offset, max_bytes - offset, current_rip);
		if (decoded.length == 0 || decoded.opcode == X86Opcode::Invalid) break;

		IROpcode ir_op = IROpcode::Nop;
		switch (decoded.opcode) {
			case X86Opcode::Nop:    ir_op = IROpcode::Nop; break;
			case X86Opcode::Mov:    ir_op = IROpcode::Add; break; // MOV mapped to Add dst, src, 0 or direct value
			case X86Opcode::Add:    ir_op = IROpcode::Add; break;
			case X86Opcode::Sub:    ir_op = IROpcode::Sub; break;
			case X86Opcode::Imul:   ir_op = IROpcode::Mul; break;
			case X86Opcode::And:    ir_op = IROpcode::And; break;
			case X86Opcode::Or:     ir_op = IROpcode::Or; break;
			case X86Opcode::Xor:    ir_op = IROpcode::Xor; break;
			case X86Opcode::Shl:    ir_op = IROpcode::Shl; break;
			case X86Opcode::Shr:    ir_op = IROpcode::LShr; break;
			case X86Opcode::Sar:    ir_op = IROpcode::AShr; break;
			case X86Opcode::Ret:    ir_op = IROpcode::Return; break;
			case X86Opcode::Jmp:    ir_op = IROpcode::Jump; break;
			case X86Opcode::Jcc:    ir_op = IROpcode::BranchCond; break;
			default:                ir_op = IROpcode::Nop; break;
		}

		auto inst = std::make_unique<IRInstruction>(ir_op);
		inst->SetGuestRip(current_rip);

		if (decoded.dst.kind == X86Operand::Kind::Reg) {
			VirtualReg dst_vr = cfg->AllocateVReg(decoded.dst.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
			dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
			inst->SetDst(dst_vr);
		}

		Value src_val = MapX86OperandToIR(*cfg, entry_bb, decoded.src);
		inst->AddOperand(src_val);

		entry_bb->AddInstruction(std::move(inst));

		offset += decoded.length;
		current_rip += decoded.length;
		count++;

		if (decoded.opcode == X86Opcode::Ret || decoded.opcode == X86Opcode::Jmp) break;
	}

	return cfg;
}

} // namespace Loader::Recompiler
