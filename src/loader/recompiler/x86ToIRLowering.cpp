// x86ToIRLowering.cpp
//
// Front-End x86-64 to Target-Independent Compiler IR Lowering Engine.

#include "loader/recompiler/x86ToIRLowering.h"

namespace Loader::Recompiler {

Value X86ToIRLowering::MapX86OperandToIR(ControlFlowGraph& cfg, BasicBlock* bb, const X86Operand& op) {
	if (op.kind == X86Operand::Kind::Reg) {
		DataType dt = (op.size_bytes == 16) ? DataType::Vec128 : (op.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
		VirtualReg vr = cfg.AllocateVReg(dt);
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

static IRCondition MapX86ConditionToIR(X86Condition cond, bool invert) noexcept {
	switch (cond) {
		case X86Condition::Equal:        return invert ? IRCondition::NotEqual : IRCondition::Equal;
		case X86Condition::Carry:        return invert ? IRCondition::AboveOrEqual : IRCondition::Below;
		case X86Condition::BelowOrEqual: return invert ? IRCondition::Above : IRCondition::BelowOrEqual;
		case X86Condition::Less:         return invert ? IRCondition::GreaterOrEqual : IRCondition::Less;
		case X86Condition::LessOrEqual:  return invert ? IRCondition::Greater : IRCondition::LessOrEqual;
		case X86Condition::Overflow:     return invert ? IRCondition::NoOverflow : IRCondition::Overflow;
		case X86Condition::Sign:         return invert ? IRCondition::GreaterOrEqual : IRCondition::Less;
		default:                         return invert ? IRCondition::NotEqual : IRCondition::Equal;
	}
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

		switch (decoded.opcode) {
			case X86Opcode::Nop: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Nop);
				inst->SetGuestRip(current_rip);
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Mov: {
				if (decoded.dst.kind == X86Operand::Kind::Reg && decoded.src.kind == X86Operand::Kind::Mem) {
					// Load: dst = [mem]
					auto inst = std::make_unique<IRInstruction>(IROpcode::Load);
					inst->SetGuestRip(current_rip);
					VirtualReg dst_vr = cfg->AllocateVReg(decoded.dst.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					entry_bb->AddInstruction(std::move(inst));
				} else if (decoded.dst.kind == X86Operand::Kind::Mem) {
					// Store: [mem] = src
					auto inst = std::make_unique<IRInstruction>(IROpcode::Store);
					inst->SetGuestRip(current_rip);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
					entry_bb->AddInstruction(std::move(inst));
				} else if (decoded.dst.kind == X86Operand::Kind::Reg) {
					// Reg Mov: dst = src
					auto inst = std::make_unique<IRInstruction>(IROpcode::Add);
					inst->SetGuestRip(current_rip);
					VirtualReg dst_vr = cfg->AllocateVReg(decoded.dst.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					entry_bb->AddInstruction(std::move(inst));
				}
				break;
			}

			case X86Opcode::Movaps:
			case X86Opcode::Movups:
			case X86Opcode::Movdqa:
			case X86Opcode::Movdqu: {
				if (decoded.dst.kind == X86Operand::Kind::Reg && decoded.src.kind == X86Operand::Kind::Mem) {
					auto inst = std::make_unique<IRInstruction>(IROpcode::VecLoad);
					inst->SetGuestRip(current_rip);
					VirtualReg dst_vr = cfg->AllocateVReg(DataType::Vec128);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					entry_bb->AddInstruction(std::move(inst));
				} else if (decoded.dst.kind == X86Operand::Kind::Mem) {
					auto inst = std::make_unique<IRInstruction>(IROpcode::VecStore);
					inst->SetGuestRip(current_rip);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
					entry_bb->AddInstruction(std::move(inst));
				} else if (decoded.dst.kind == X86Operand::Kind::Reg) {
					auto inst = std::make_unique<IRInstruction>(IROpcode::VecMov);
					inst->SetGuestRip(current_rip);
					VirtualReg dst_vr = cfg->AllocateVReg(DataType::Vec128);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					entry_bb->AddInstruction(std::move(inst));
				}
				break;
			}

			case X86Opcode::Addps:
			case X86Opcode::Subps:
			case X86Opcode::Mulps:
			case X86Opcode::Divps:
			case X86Opcode::Sqrtps:
			case X86Opcode::Andps:
			case X86Opcode::Orps:
			case X86Opcode::Xorps:
			case X86Opcode::Minps:
			case X86Opcode::Maxps:
			case X86Opcode::Pand:
			case X86Opcode::Por:
			case X86Opcode::Pxor:
			case X86Opcode::Paddd:
			case X86Opcode::Psubd: {
				IROpcode ir_op = IROpcode::VecAdd;
				if (decoded.opcode == X86Opcode::Subps || decoded.opcode == X86Opcode::Psubd) ir_op = IROpcode::VecSub;
				else if (decoded.opcode == X86Opcode::Mulps) ir_op = IROpcode::VecMul;
				else if (decoded.opcode == X86Opcode::Divps) ir_op = IROpcode::VecDiv;
				else if (decoded.opcode == X86Opcode::Sqrtps) ir_op = IROpcode::VecSqrt;
				else if (decoded.opcode == X86Opcode::Andps || decoded.opcode == X86Opcode::Pand) ir_op = IROpcode::VecAnd;
				else if (decoded.opcode == X86Opcode::Orps || decoded.opcode == X86Opcode::Por) ir_op = IROpcode::VecOr;
				else if (decoded.opcode == X86Opcode::Xorps || decoded.opcode == X86Opcode::Pxor) ir_op = IROpcode::VecXor;
				else if (decoded.opcode == X86Opcode::Minps) ir_op = IROpcode::VecMin;
				else if (decoded.opcode == X86Opcode::Maxps) ir_op = IROpcode::VecMax;

				auto inst = std::make_unique<IRInstruction>(ir_op);
				inst->SetGuestRip(current_rip);

				if (decoded.dst.kind == X86Operand::Kind::Reg) {
					VirtualReg dst_vr = cfg->AllocateVReg(DataType::Vec128);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					if (ir_op == IROpcode::VecSqrt) {
						inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src.kind != X86Operand::Kind::None ? decoded.src : decoded.dst));
					} else {
						inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
						inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
					}
				}
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Add:
			case X86Opcode::Sub:
			case X86Opcode::Imul:
			case X86Opcode::And:
			case X86Opcode::Or:
			case X86Opcode::Xor:
			case X86Opcode::Shl:
			case X86Opcode::Shr:
			case X86Opcode::Sar: {
				IROpcode ir_op = IROpcode::Add;
				if (decoded.opcode == X86Opcode::Sub) ir_op = IROpcode::Sub;
				else if (decoded.opcode == X86Opcode::Imul) ir_op = IROpcode::Mul;
				else if (decoded.opcode == X86Opcode::And) ir_op = IROpcode::And;
				else if (decoded.opcode == X86Opcode::Or) ir_op = IROpcode::Or;
				else if (decoded.opcode == X86Opcode::Xor) ir_op = IROpcode::Xor;
				else if (decoded.opcode == X86Opcode::Shl) ir_op = IROpcode::Shl;
				else if (decoded.opcode == X86Opcode::Shr) ir_op = IROpcode::LShr;
				else if (decoded.opcode == X86Opcode::Sar) ir_op = IROpcode::AShr;

				auto inst = std::make_unique<IRInstruction>(ir_op);
				inst->SetGuestRip(current_rip);

				if (decoded.dst.kind == X86Operand::Kind::Reg) {
					VirtualReg dst_vr = cfg->AllocateVReg(decoded.dst.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					inst->SetDst(dst_vr);
					// 2-operand semantic: dst = dst op src
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
					inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
				}
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Cmp: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Cmp);
				inst->SetGuestRip(current_rip);
				inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
				inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Test: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Test);
				inst->SetGuestRip(current_rip);
				inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst));
				inst->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.src));
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Push: {
				// PUSH src -> RSP -= 8; STORE src, [RSP]
				VirtualReg rsp_vr = cfg->AllocateVReg(DataType::Int64);
				rsp_vr.phys_pin = static_cast<int8_t>(X86Reg::RSP);

				auto sub_rsp = std::make_unique<IRInstruction>(IROpcode::Sub);
				sub_rsp->SetGuestRip(current_rip);
				sub_rsp->SetDst(rsp_vr);
				sub_rsp->AddOperand(Value::MakeVReg(rsp_vr));
				sub_rsp->AddOperand(Value::MakeImmInt(8));
				entry_bb->AddInstruction(std::move(sub_rsp));

				auto store = std::make_unique<IRInstruction>(IROpcode::Store);
				store->SetGuestRip(current_rip);
				store->AddOperand(MapX86OperandToIR(*cfg, entry_bb, decoded.dst.kind != X86Operand::Kind::None ? decoded.dst : decoded.src));
				VirtualReg rsp_base = rsp_vr;
				VirtualReg none_idx{};
				store->AddOperand(Value::MakeMemory(rsp_base, none_idx, 1, 0));
				entry_bb->AddInstruction(std::move(store));
				break;
			}

			case X86Opcode::Pop: {
				// POP dst -> LOAD dst, [RSP]; RSP += 8
				VirtualReg rsp_vr = cfg->AllocateVReg(DataType::Int64);
				rsp_vr.phys_pin = static_cast<int8_t>(X86Reg::RSP);

				if (decoded.dst.kind == X86Operand::Kind::Reg) {
					VirtualReg dst_vr = cfg->AllocateVReg(decoded.dst.size_bytes == 4 ? DataType::Int32 : DataType::Int64);
					dst_vr.phys_pin = static_cast<int8_t>(decoded.dst.reg);
					auto load = std::make_unique<IRInstruction>(IROpcode::Load);
					load->SetGuestRip(current_rip);
					load->SetDst(dst_vr);
					VirtualReg none_idx{};
					load->AddOperand(Value::MakeMemory(rsp_vr, none_idx, 1, 0));
					entry_bb->AddInstruction(std::move(load));
				}

				auto add_rsp = std::make_unique<IRInstruction>(IROpcode::Add);
				add_rsp->SetGuestRip(current_rip);
				add_rsp->SetDst(rsp_vr);
				add_rsp->AddOperand(Value::MakeVReg(rsp_vr));
				add_rsp->AddOperand(Value::MakeImmInt(8));
				entry_bb->AddInstruction(std::move(add_rsp));
				break;
			}

			case X86Opcode::Jcc: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::BranchCond);
				inst->SetGuestRip(current_rip);
				inst->SetCondition(MapX86ConditionToIR(decoded.cond, decoded.cond_invert));
				uint64_t target_rip = current_rip + decoded.length + decoded.dst.imm;
				inst->AddOperand(Value::MakeImmInt(target_rip));
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Jmp: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Jump);
				inst->SetGuestRip(current_rip);
				uint64_t target_rip = current_rip + decoded.length + decoded.dst.imm;
				inst->AddOperand(Value::MakeImmInt(target_rip));
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			case X86Opcode::Ret: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Return);
				inst->SetGuestRip(current_rip);
				entry_bb->AddInstruction(std::move(inst));
				break;
			}

			default: {
				auto inst = std::make_unique<IRInstruction>(IROpcode::Nop);
				inst->SetGuestRip(current_rip);
				entry_bb->AddInstruction(std::move(inst));
				break;
			}
		}

		offset += decoded.length;
		current_rip += decoded.length;
		count++;

		if (decoded.opcode == X86Opcode::Ret || decoded.opcode == X86Opcode::Jmp) break;
	}

	return cfg;
}

} // namespace Loader::Recompiler
