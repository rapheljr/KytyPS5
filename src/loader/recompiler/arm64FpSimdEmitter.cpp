// arm64FpSimdEmitter.cpp
//
// ARM64 Floating-Point Scalar & 128-Bit NEON Vector SIMD Instruction Emitter.

#include "loader/recompiler/arm64FpSimdEmitter.h"

namespace Loader::Recompiler {

// ─── Scalar FP Single-Precision Instructions ─────────────────────────────────

void Arm64FpSimdEmitter::EmitFaddS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x1E202800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFsubS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x1E203800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmulS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x1E200800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFdivS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x1E201800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 1. Vector Loads & Stores (128-Bit Q-Regs & NEON Element) ─────────────────

void Arm64FpSimdEmitter::EmitLdrQ(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	uint32_t imm12 = (static_cast<uint32_t>(offset / 16) & 0xFFFu);
	uint32_t inst = 0x3DC00000u | (imm12 << 10) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitStrQ(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	uint32_t imm12 = (static_cast<uint32_t>(offset / 16) & 0xFFFu);
	uint32_t inst = 0x3D800000u | (imm12 << 10) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitLd116B(Arm64FpReg rt, Arm64Reg rn) {
	uint32_t inst = 0x4C407000u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitSt116B(Arm64FpReg rt, Arm64Reg rn) {
	uint32_t inst = 0x4C007000u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 2. Vector Floating-Point Operations (.4S) ────────────────────────────────

void Arm64FpSimdEmitter::EmitVadd4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E20D400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVsub4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA0D400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVmul4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6E20DC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVdiv4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6E20FC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVFaddp4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6E20D400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmax4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E20F400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmin4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA0F400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFsqrt4S(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x6EA1F800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmla4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E20CC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmls4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA0CC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmla2D(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EE0CC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmls2D(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4F60CC00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmaddS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm, Arm64FpReg ra) {
	uint32_t inst = 0x1F000000u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(ra) & 0x1Fu) << 10) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmsubS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm, Arm64FpReg ra) {
	uint32_t inst = 0x1F008000u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(ra) & 0x1Fu) << 10) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 3. Vector Integer Operations (.4S / .16B) ────────────────────────────────

void Arm64FpSimdEmitter::EmitVadd16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E208400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVsub16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA08400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitAdd4S_Int(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA08400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitSub4S_Int(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6EA08400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitAbs4S(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x4EA0B800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitSmax4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E206400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitSmin4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA06400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 4. Vector Shuffles & Permutations ───────────────────────────────────────

void Arm64FpSimdEmitter::EmitTbl16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E000000u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitZip14S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E803800u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitZip24S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E807800u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 5. Vector Logic & Mask Operations ────────────────────────────────────────

void Arm64FpSimdEmitter::EmitVand16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E201C00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVorr16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4EA01C00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVeor16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6E201C00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVbsl16B(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x6E601C00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitCmeq4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E208C00u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitCmgt4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	uint32_t inst = 0x4E203400u | ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitCnt8B(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x0E205800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitUaddlv8B(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x2E303800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitIns4S(Arm64FpReg rd, uint8_t lane, Arm64Reg rn) {
	uint32_t imm5 = 0x04u | ((lane & 3u) << 3u);
	uint32_t inst = 0x4E001C00u | (imm5 << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitUmov4S(Arm64Reg rd, Arm64FpReg rn, uint8_t lane) {
	uint32_t imm5 = 0x04u | ((lane & 3u) << 3u);
	uint32_t inst = 0x0E003C00u | (imm5 << 16) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 6. Vector Conversions ────────────────────────────────────────────────────

void Arm64FpSimdEmitter::EmitFcvtns4S(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x4E21A800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitScvtf4S(Arm64FpReg rd, Arm64FpReg rn) {
	uint32_t inst = 0x4E21D800u | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── Scalar FP Load & Store Instructions ─────────────────────────────────────

void Arm64FpSimdEmitter::EmitLdrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	uint32_t imm12 = (static_cast<uint32_t>(offset / 4) & 0xFFFu);
	uint32_t inst = 0xBD400000u | (imm12 << 10) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitStrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	uint32_t imm12 = (static_cast<uint32_t>(offset / 4) & 0xFFFu);
	uint32_t inst = 0xBD000000u | (imm12 << 10) | ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) | (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

} // namespace Loader::Recompiler
