// arm64Emitter.cpp
//
// ARM64 Backend Code Emitter & Instruction Encoder for Phase M Dynamic Recompiler.

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/arm64EncoderHelpers.h"

namespace Loader::Recompiler {

using namespace Arm64EncoderHelper;

Arm64Reg Arm64Emitter::MapX86ToArm64Reg(X86Reg reg) noexcept {
	switch (reg) {
		case X86Reg::RAX: return Arm64Reg::X0;
		case X86Reg::RCX: return Arm64Reg::X1;
		case X86Reg::RDX: return Arm64Reg::X2;
		case X86Reg::RBX: return Arm64Reg::X3;
		case X86Reg::RSP: return Arm64Reg::X4;
		case X86Reg::RBP: return Arm64Reg::X5;
		case X86Reg::RSI: return Arm64Reg::X6;
		case X86Reg::RDI: return Arm64Reg::X7;
		case X86Reg::R8:  return Arm64Reg::X8;
		case X86Reg::R9:  return Arm64Reg::X9;
		case X86Reg::R10: return Arm64Reg::X10;
		case X86Reg::R11: return Arm64Reg::X11;
		case X86Reg::R12: return Arm64Reg::X12;
		case X86Reg::R13: return Arm64Reg::X13;
		case X86Reg::R14: return Arm64Reg::X14;
		case X86Reg::R15: return Arm64Reg::X15;
		default:          return Arm64Reg::X0;
	}
}

void Arm64Emitter::Emit32(uint32_t inst) {
	m_code.push_back(inst);
}

void Arm64Emitter::EmitNop() {
	Emit32(0xD503201Fu);
}

// 1. Integer Instructions
void Arm64Emitter::EmitAndReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(LogicReg(sf, 0, false, Reg(src2), 0, Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitOrrReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(LogicReg(sf, 1, false, Reg(src2), 0, Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitEorReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(LogicReg(sf, 2, false, Reg(src2), 0, Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitMovReg(Arm64Reg dst, Arm64Reg src, bool sf) {
	if (dst == src) return; // Redundant move elimination
	EmitOrrReg(dst, Arm64Reg::XZR, src, sf);
}

void Arm64Emitter::EmitMovz(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw, bool sf) {
	Emit32(MoveWide(sf, 2, shift_hw, imm16, Reg(dst)));
}

void Arm64Emitter::EmitMovk(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw, bool sf) {
	Emit32(MoveWide(sf, 3, shift_hw, imm16, Reg(dst)));
}

void Arm64Emitter::EmitMovn(Arm64Reg dst, uint16_t imm16, uint8_t shift_hw, bool sf) {
	Emit32(MoveWide(sf, 0, shift_hw, imm16, Reg(dst)));
}

void Arm64Emitter::EmitMovImm64(Arm64Reg dst, uint64_t imm) {
	// Optimized immediate sequence (skip zero half-words)
	uint16_t h0 = imm & 0xFFFFu;
	uint16_t h1 = (imm >> 16u) & 0xFFFFu;
	uint16_t h2 = (imm >> 32u) & 0xFFFFu;
	uint16_t h3 = (imm >> 48u) & 0xFFFFu;

	if (imm == ~0ULL) {
		EmitMovn(dst, 0, 0, true);
		return;
	}

	bool initialized = false;
	if (h0 != 0 || (h1 == 0 && h2 == 0 && h3 == 0)) {
		EmitMovz(dst, h0, 0, true);
		initialized = true;
	}
	if (h1 != 0) {
		if (!initialized) { EmitMovz(dst, h1, 1, true); initialized = true; }
		else EmitMovk(dst, h1, 1, true);
	}
	if (h2 != 0) {
		if (!initialized) { EmitMovz(dst, h2, 2, true); initialized = true; }
		else EmitMovk(dst, h2, 2, true);
	}
	if (h3 != 0) {
		if (!initialized) { EmitMovz(dst, h3, 3, true); initialized = true; }
		else EmitMovk(dst, h3, 3, true);
	}
}

void Arm64Emitter::EmitAddReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(AddSubReg(sf, false, false, Reg(src2), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitAddImm(Arm64Reg dst, Arm64Reg src, uint32_t imm, bool sf) {
	if (imm == 0 && dst == src) return; // Redundant add 0 elimination
	Emit32(AddSubImm(sf, false, false, imm, Reg(src), Reg(dst)));
}

void Arm64Emitter::EmitSubReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(AddSubReg(sf, true, false, Reg(src2), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitSubImm(Arm64Reg dst, Arm64Reg src, uint32_t imm, bool sf) {
	if (imm == 0 && dst == src) return;
	Emit32(AddSubImm(sf, true, false, imm, Reg(src), Reg(dst)));
}

void Arm64Emitter::EmitAdc(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(AddSubCarry(sf, false, false, Reg(src2), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitSbc(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(AddSubCarry(sf, true, false, Reg(src2), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitCmpReg(Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(AddSubReg(sf, true, true, Reg(src2), Reg(src1), 31)); // SUBS XZR, Xn, Xm
}

void Arm64Emitter::EmitCmpImm(Arm64Reg src, uint32_t imm, bool sf) {
	Emit32(AddSubImm(sf, true, true, imm, Reg(src), 31)); // SUBS XZR, Xn, #imm
}

void Arm64Emitter::EmitTstReg(Arm64Reg src1, Arm64Reg src2, bool sf) {
	Emit32(LogicReg(sf, 3, false, Reg(src2), 0, Reg(src1), 31)); // ANDS XZR, Xn, Xm
}

// 2. Memory Instructions
void Arm64Emitter::EmitLdr64(Arm64Reg dst, Arm64Reg base, int32_t offset) {
	if (offset < 0 || (offset % 8 != 0)) {
		EmitLdur64(dst, base, offset);
		return;
	}
	uint32_t imm12 = static_cast<uint32_t>(offset / 8);
	Emit32(LoadStoreUnsigned(true, true, imm12, Reg(base), Reg(dst)));
}

void Arm64Emitter::EmitStr64(Arm64Reg src, Arm64Reg base, int32_t offset) {
	if (offset < 0 || (offset % 8 != 0)) {
		EmitStur64(src, base, offset);
		return;
	}
	uint32_t imm12 = static_cast<uint32_t>(offset / 8);
	Emit32(LoadStoreUnsigned(true, false, imm12, Reg(base), Reg(src)));
}

void Arm64Emitter::EmitLdur64(Arm64Reg dst, Arm64Reg base, int32_t simm9) {
	Emit32(LoadStoreUnscaled(true, true, simm9, Reg(base), Reg(dst)));
}

void Arm64Emitter::EmitStur64(Arm64Reg src, Arm64Reg base, int32_t simm9) {
	Emit32(LoadStoreUnscaled(true, false, simm9, Reg(base), Reg(src)));
}

void Arm64Emitter::EmitLdp64(Arm64Reg dst1, Arm64Reg dst2, Arm64Reg base, int32_t offset) {
	int32_t simm7 = offset / 8;
	Emit32(LoadStorePair(true, true, simm7, Reg(base), Reg(dst1), Reg(dst2)));
}

void Arm64Emitter::EmitStp64(Arm64Reg src1, Arm64Reg src2, Arm64Reg base, int32_t offset) {
	int32_t simm7 = offset / 8;
	Emit32(LoadStorePair(true, false, simm7, Reg(base), Reg(src1), Reg(src2)));
}

// 3. Branch Instructions
void Arm64Emitter::EmitB(int32_t offset_words) {
	Emit32(BranchUncond(false, offset_words));
}

void Arm64Emitter::EmitBl(int32_t offset_words) {
	Emit32(BranchUncond(true, offset_words));
}

void Arm64Emitter::EmitBr(Arm64Reg reg) {
	Emit32(0xD61F0000u | (Reg(reg) << 5u));
}

void Arm64Emitter::EmitBlr(Arm64Reg reg) {
	Emit32(0xD63F0000u | (Reg(reg) << 5u));
}

void Arm64Emitter::EmitRet() {
	Emit32(0xD65F03C0u);
}

void Arm64Emitter::EmitCbz(Arm64Reg reg, int32_t offset_words, bool sf) {
	Emit32(CompareBranch(sf, false, offset_words, Reg(reg)));
}

void Arm64Emitter::EmitCbnz(Arm64Reg reg, int32_t offset_words, bool sf) {
	Emit32(CompareBranch(sf, true, offset_words, Reg(reg)));
}

void Arm64Emitter::EmitTbz(Arm64Reg reg, uint8_t bit_num, int32_t offset_words) {
	Emit32(TestBranch(false, bit_num, offset_words, Reg(reg)));
}

void Arm64Emitter::EmitTbnz(Arm64Reg reg, uint8_t bit_num, int32_t offset_words) {
	Emit32(TestBranch(true, bit_num, offset_words, Reg(reg)));
}

void Arm64Emitter::EmitBcc(X86Condition cond, bool invert, int32_t offset_words) {
	uint32_t cond_code = 0;
	switch (cond) {
		case X86Condition::Equal:        cond_code = invert ? 0x1 : 0x0; break; // EQ / NE
		case X86Condition::Carry:        cond_code = invert ? 0x3 : 0x2; break; // CS / CC
		case X86Condition::Sign:         cond_code = invert ? 0xB : 0xA; break; // MI / PL
		case X86Condition::Less:         cond_code = invert ? 0xD : 0xC; break; // LT / GE
		case X86Condition::LessOrEqual:  cond_code = invert ? 0xF : 0xE; break; // LE / GT
		default:                         cond_code = 0x0; break;
	}
	Emit32(BranchCond(cond_code, offset_words));
}

// 4. Shift Instructions
void Arm64Emitter::EmitLsl(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf) {
	Emit32(ShiftVariable(sf, 0, Reg(shift), Reg(src), Reg(dst)));
}

void Arm64Emitter::EmitLsr(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf) {
	Emit32(ShiftVariable(sf, 1, Reg(shift), Reg(src), Reg(dst)));
}

void Arm64Emitter::EmitAsr(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf) {
	Emit32(ShiftVariable(sf, 2, Reg(shift), Reg(src), Reg(dst)));
}

void Arm64Emitter::EmitRor(Arm64Reg dst, Arm64Reg src, Arm64Reg shift, bool sf) {
	Emit32(ShiftVariable(sf, 3, Reg(shift), Reg(src), Reg(dst)));
}

// 5. Multiply Instructions
void Arm64Emitter::EmitMulReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, bool sf) {
	EmitMadd(dst, src1, src2, Arm64Reg::XZR, sf);
}

void Arm64Emitter::EmitMadd(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, Arm64Reg acc, bool sf) {
	Emit32(Multiply(sf, false, Reg(src2), Reg(acc), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitMsub(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2, Arm64Reg acc, bool sf) {
	Emit32(Multiply(sf, true, Reg(src2), Reg(acc), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitUmulh(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
	Emit32(MultiplyHigh(false, Reg(src2), Reg(src1), Reg(dst)));
}

void Arm64Emitter::EmitSmulh(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
	Emit32(MultiplyHigh(true, Reg(src2), Reg(src1), Reg(dst)));
}

bool Arm64Emitter::CompileBlock(const RecompilerBasicBlock& block) {
	const auto& insts = block.GetInstructions();

	for (const auto& inst : insts) {
		if (!inst.active) continue;

		switch (inst.opcode) {
			case X86Opcode::Nop:
				EmitNop();
				break;

			case X86Opcode::Mov:
				if (inst.dst.kind == X86Operand::Kind::Reg) {
					Arm64Reg dst_arm = MapX86ToArm64Reg(inst.dst.reg);
					if (inst.src.kind == X86Operand::Kind::Reg) {
						EmitMovReg(dst_arm, MapX86ToArm64Reg(inst.src.reg));
					} else if (inst.src.kind == X86Operand::Kind::Imm) {
						EmitMovImm64(dst_arm, static_cast<uint64_t>(inst.src.imm));
					}
				}
				break;

			case X86Opcode::Add:
				if (inst.dst.kind == X86Operand::Kind::Reg) {
					Arm64Reg dst_arm = MapX86ToArm64Reg(inst.dst.reg);
					if (inst.src.kind == X86Operand::Kind::Reg) {
						EmitAddReg(dst_arm, dst_arm, MapX86ToArm64Reg(inst.src.reg));
					} else if (inst.src.kind == X86Operand::Kind::Imm) {
						EmitAddImm(dst_arm, dst_arm, static_cast<uint32_t>(inst.src.imm));
					}
				}
				break;

			case X86Opcode::Sub:
				if (inst.dst.kind == X86Operand::Kind::Reg) {
					Arm64Reg dst_arm = MapX86ToArm64Reg(inst.dst.reg);
					if (inst.src.kind == X86Operand::Kind::Reg) {
						EmitSubReg(dst_arm, dst_arm, MapX86ToArm64Reg(inst.src.reg));
					} else if (inst.src.kind == X86Operand::Kind::Imm) {
						EmitSubImm(dst_arm, dst_arm, static_cast<uint32_t>(inst.src.imm));
					}
				}
				break;

			case X86Opcode::Imul:
				if (inst.dst.kind == X86Operand::Kind::Reg && inst.src.kind == X86Operand::Kind::Reg) {
					Arm64Reg dst_arm = MapX86ToArm64Reg(inst.dst.reg);
					EmitMulReg(dst_arm, dst_arm, MapX86ToArm64Reg(inst.src.reg));
				}
				break;

			case X86Opcode::Cmp:
				if (inst.dst.kind == X86Operand::Kind::Reg) {
					Arm64Reg dst_arm = MapX86ToArm64Reg(inst.dst.reg);
					if (inst.src.kind == X86Operand::Kind::Reg) {
						EmitCmpReg(dst_arm, MapX86ToArm64Reg(inst.src.reg));
					} else if (inst.src.kind == X86Operand::Kind::Imm) {
						EmitCmpImm(dst_arm, static_cast<uint32_t>(inst.src.imm));
					}
				}
				break;

			case X86Opcode::Ret:
				EmitRet();
				break;

			default:
				EmitNop();
				break;
		}
	}

	return true;
}

} // namespace Loader::Recompiler
