// arm64Emitter.cpp
//
// ARM64 Backend Code Emitter & Instruction Encoder for Phase M Dynamic Recompiler.

#include "loader/recompiler/arm64Emitter.h"

namespace Loader::Recompiler {

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
	Emit32(0xD503201F);
}

void Arm64Emitter::EmitRet() {
	Emit32(0xD65F03C0);
}

void Arm64Emitter::EmitMovReg(Arm64Reg dst, Arm64Reg src) {
	if (dst == src) return;
	// ORR Xdst, XZR, Xsrc -> 0xAA0003E0 | (src << 16) | dst
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s = static_cast<uint32_t>(src);
	Emit32(0xAA0003E0u | (s << 16u) | d);
}

void Arm64Emitter::EmitMovImm64(Arm64Reg dst, uint64_t imm) {
	uint32_t d = static_cast<uint32_t>(dst);
	// MOVZ Xd, imm[0:15]
	Emit32(0xD2800000u | ((imm & 0xFFFFu) << 5u) | d);
	// MOVK Xd, imm[16:31], LSL #16
	Emit32(0xF2A00000u | (((imm >> 16u) & 0xFFFFu) << 5u) | d);
	// MOVK Xd, imm[32:47], LSL #32
	Emit32(0xF2C00000u | (((imm >> 32u) & 0xFFFFu) << 5u) | d);
	// MOVK Xd, imm[48:63], LSL #48
	Emit32(0xF2E00000u | (((imm >> 48u) & 0xFFFFu) << 5u) | d);
}

void Arm64Emitter::EmitAddReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s1 = static_cast<uint32_t>(src1);
	uint32_t s2 = static_cast<uint32_t>(src2);
	// ADD Xd, Xs1, Xs2 -> 0x8B000000 | (s2 << 16) | (s1 << 5) | d
	Emit32(0x8B000000u | (s2 << 16u) | (s1 << 5u) | d);
}

void Arm64Emitter::EmitAddImm(Arm64Reg dst, Arm64Reg src, uint32_t imm) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s = static_cast<uint32_t>(src);
	uint32_t imm12 = imm & 0xFFFu;
	// ADD Xd, Xs, #imm12 -> 0x91000000 | (imm12 << 10) | (s << 5) | d
	Emit32(0x91000000u | (imm12 << 10u) | (s << 5u) | d);
}

void Arm64Emitter::EmitSubReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s1 = static_cast<uint32_t>(src1);
	uint32_t s2 = static_cast<uint32_t>(src2);
	// SUB Xd, Xs1, Xs2 -> 0xCB000000 | (s2 << 16) | (s1 << 5) | d
	Emit32(0xCB000000u | (s2 << 16u) | (s1 << 5u) | d);
}

void Arm64Emitter::EmitSubImm(Arm64Reg dst, Arm64Reg src, uint32_t imm) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s = static_cast<uint32_t>(src);
	uint32_t imm12 = imm & 0xFFFu;
	// SUB Xd, Xs, #imm12 -> 0xD1000000 | (imm12 << 10) | (s << 5) | d
	Emit32(0xD1000000u | (imm12 << 10u) | (s << 5u) | d);
}

void Arm64Emitter::EmitMulReg(Arm64Reg dst, Arm64Reg src1, Arm64Reg src2) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t s1 = static_cast<uint32_t>(src1);
	uint32_t s2 = static_cast<uint32_t>(src2);
	// MUL Xd, Xs1, Xs2 -> MADD Xd, Xs1, Xs2, XZR -> 0x9B007C00 | (s2 << 16) | (s1 << 5) | d
	Emit32(0x9B007C00u | (s2 << 16u) | (s1 << 5u) | d);
}

void Arm64Emitter::EmitCmpReg(Arm64Reg src1, Arm64Reg src2) {
	uint32_t s1 = static_cast<uint32_t>(src1);
	uint32_t s2 = static_cast<uint32_t>(src2);
	// SUBS XZR, Xs1, Xs2 -> 0xEB00001F | (s2 << 16) | (s1 << 5)
	Emit32(0xEB00001Fu | (s2 << 16u) | (s1 << 5u));
}

void Arm64Emitter::EmitCmpImm(Arm64Reg src, uint32_t imm) {
	uint32_t s = static_cast<uint32_t>(src);
	uint32_t imm12 = imm & 0xFFFu;
	// SUBS XZR, Xs, #imm12 -> 0xF100001F | (imm12 << 10) | (s << 5)
	Emit32(0xF100001Fu | (imm12 << 10u) | (s << 5u));
}

void Arm64Emitter::EmitB(int32_t offset_words) {
	uint32_t imm26 = static_cast<uint32_t>(offset_words) & 0x03FFFFFFu;
	// B offset -> 0x14000000 | imm26
	Emit32(0x14000000u | imm26);
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
	uint32_t imm19 = static_cast<uint32_t>(offset_words) & 0x7FFFFu;
	// B.cond offset -> 0x54000000 | (imm19 << 5) | cond_code
	Emit32(0x54000000u | (imm19 << 5u) | cond_code);
}

void Arm64Emitter::EmitBlr(Arm64Reg reg) {
	uint32_t r = static_cast<uint32_t>(reg);
	// BLR Xr -> 0xD63F0000 | (r << 5)
	Emit32(0xD63F0000u | (r << 5u));
}

void Arm64Emitter::EmitLdr64(Arm64Reg dst, Arm64Reg base, int32_t offset) {
	uint32_t d = static_cast<uint32_t>(dst);
	uint32_t b = static_cast<uint32_t>(base);
	uint32_t imm12 = (static_cast<uint32_t>(offset) / 8u) & 0xFFFu;
	// LDR Xd, [Xb, #offset] -> 0xF9400000 | (imm12 << 10) | (b << 5) | d
	Emit32(0xF9400000u | (imm12 << 10u) | (b << 5u) | d);
}

void Arm64Emitter::EmitStr64(Arm64Reg src, Arm64Reg base, int32_t offset) {
	uint32_t s = static_cast<uint32_t>(src);
	uint32_t b = static_cast<uint32_t>(base);
	uint32_t imm12 = (static_cast<uint32_t>(offset) / 8u) & 0xFFFu;
	// STR Xs, [Xb, #offset] -> 0xF9000000 | (imm12 << 10) | (b << 5) | s
	Emit32(0xF9000000u | (imm12 << 10u) | (b << 5u) | s);
}

void Arm64Emitter::EmitStp64(Arm64Reg src1, Arm64Reg src2, Arm64Reg base, int32_t offset) {
	uint32_t s1 = static_cast<uint32_t>(src1);
	uint32_t s2 = static_cast<uint32_t>(src2);
	uint32_t b  = static_cast<uint32_t>(base);
	uint32_t imm7 = (static_cast<uint32_t>(offset) / 8u) & 0x7Fu;
	// STP Xs1, Xs2, [Xb, #offset] -> 0xA9000000 | (imm7 << 15) | (s2 << 10) | (b << 5) | s1
	Emit32(0xA9000000u | (imm7 << 15u) | (s2 << 10u) | (b << 5u) | s1);
}

void Arm64Emitter::EmitLdp64(Arm64Reg dst1, Arm64Reg dst2, Arm64Reg base, int32_t offset) {
	uint32_t d1 = static_cast<uint32_t>(dst1);
	uint32_t d2 = static_cast<uint32_t>(dst2);
	uint32_t b  = static_cast<uint32_t>(base);
	uint32_t imm7 = (static_cast<uint32_t>(offset) / 8u) & 0x7Fu;
	// LDP Xd1, Xd2, [Xb, #offset] -> 0xA9400000 | (imm7 << 15) | (d2 << 10) | (b << 5) | d1
	Emit32(0xA9400000u | (imm7 << 15u) | (d2 << 10u) | (b << 5u) | d1);
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
