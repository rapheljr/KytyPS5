// arm64InstructionSelector.cpp
//
// Pattern-Matching ARM64 Instruction Selector & Optimizer Engine.

#include "loader/recompiler/arm64InstructionSelector.h"

namespace Loader::Recompiler {

bool Arm64InstructionSelector::SelectInstruction(const IRInstruction& inst, Arm64Emitter& emitter,
                                                  const std::function<Arm64Reg(const VirtualReg&)>& vreg_map) {
	if (!inst.IsActive()) return true;

	Arm64Reg dst_reg = inst.HasDst() ? vreg_map(inst.GetDst()) : Arm64Reg::XZR;
	const auto& ops = inst.GetOperands();

	switch (inst.GetOpcode()) {
		case IROpcode::Nop:
			// Redundant nop / copy suppression
			break;

		case IROpcode::Add:
			if (ops.size() >= 2) {
				if (ops[0].IsVReg() && ops[1].IsVReg()) {
					emitter.EmitAddReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
				} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
					emitter.EmitAddImm(dst_reg, vreg_map(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
				} else if (ops[0].IsImmInt()) {
					emitter.EmitMovImm64(dst_reg, static_cast<uint64_t>(ops[0].imm_int));
				}
			} else if (ops.size() == 1) {
				if (ops[0].IsVReg()) emitter.EmitMovReg(dst_reg, vreg_map(ops[0].vreg));
				else if (ops[0].IsImmInt()) emitter.EmitMovImm64(dst_reg, static_cast<uint64_t>(ops[0].imm_int));
			}
			break;

		case IROpcode::Sub:
			if (ops.size() >= 2) {
				if (ops[0].IsVReg() && ops[1].IsVReg()) {
					emitter.EmitSubReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
				} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
					emitter.EmitSubImm(dst_reg, vreg_map(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
				}
			}
			break;

		case IROpcode::Mul:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitMulReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::And:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitAndReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::Or:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitOrrReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::Xor:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitEorReg(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::Shl:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitLsl(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::LShr:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitLsr(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::AShr:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitAsr(dst_reg, vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::Load:
			if (ops.size() >= 1 && ops[0].IsMemoryRef()) {
				emitter.EmitLdr64(dst_reg, Arm64Reg::SP, ops[0].mem_ref.disp);
			}
			break;

		case IROpcode::Store:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsMemoryRef()) {
				emitter.EmitStr64(vreg_map(ops[0].vreg), Arm64Reg::SP, ops[1].mem_ref.disp);
			}
			break;

		case IROpcode::Cmp:
			if (ops.size() >= 2) {
				if (ops[0].IsVReg() && ops[1].IsVReg()) {
					emitter.EmitCmpReg(vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
				} else if (ops[0].IsVReg() && ops[1].IsImmInt()) {
					emitter.EmitCmpImm(vreg_map(ops[0].vreg), static_cast<uint32_t>(ops[1].imm_int));
				}
			}
			break;

		case IROpcode::Test:
			if (ops.size() >= 2 && ops[0].IsVReg() && ops[1].IsVReg()) {
				emitter.EmitTstReg(vreg_map(ops[0].vreg), vreg_map(ops[1].vreg));
			}
			break;

		case IROpcode::BranchCond: {
			uint32_t cond_code = 0x0; // EQ default
			switch (inst.GetCondition()) {
				case IRCondition::Equal:          cond_code = 0x0; break;
				case IRCondition::NotEqual:       cond_code = 0x1; break;
				case IRCondition::AboveOrEqual:   cond_code = 0x2; break; // CS / HS
				case IRCondition::Below:          cond_code = 0x3; break; // CC / LO
				case IRCondition::Overflow:       cond_code = 0x6; break; // VS
				case IRCondition::NoOverflow:     cond_code = 0x7; break; // VC
				case IRCondition::Above:          cond_code = 0x8; break; // HI
				case IRCondition::BelowOrEqual:   cond_code = 0x9; break; // LS
				case IRCondition::GreaterOrEqual: cond_code = 0xA; break; // GE
				case IRCondition::Less:           cond_code = 0xB; break; // LT
				case IRCondition::Greater:        cond_code = 0xC; break; // GT
				case IRCondition::LessOrEqual:    cond_code = 0xD; break; // LE
			}
			emitter.Emit32(0x54000000u | (cond_code & 0x0Fu)); // B.cond +0 (or fallthrough)
			break;
		}

		case IROpcode::Jump:
			emitter.EmitB(0); // Branch target
			break;

		case IROpcode::Return:
			emitter.EmitRet();
			break;

		default:
			emitter.EmitNop();
			break;
	}
	return true;
}

} // namespace Loader::Recompiler
