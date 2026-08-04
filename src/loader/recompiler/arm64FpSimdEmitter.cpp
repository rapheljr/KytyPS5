// arm64FpSimdEmitter.cpp
//
// ARM64 Floating-Point Scalar & 128-Bit NEON Vector SIMD Instruction Emitter.

#include "loader/recompiler/arm64FpSimdEmitter.h"

namespace Loader::Recompiler {

// ─── Scalar FP Single-Precision Instructions ─────────────────────────────────

void Arm64FpSimdEmitter::EmitFaddS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FADD S<d>, S<n>, S<m> -> 0x1E202800
	uint32_t inst = 0x1E202800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFsubS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FSUB S<d>, S<n>, S<m> -> 0x1E203800
	uint32_t inst = 0x1E203800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFmulS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FMUL S<d>, S<n>, S<m> -> 0x1E200800
	uint32_t inst = 0x1E200800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitFdivS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FDIV S<d>, S<n>, S<m> -> 0x1E201800
	uint32_t inst = 0x1E201800u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── 128-Bit NEON Vector SIMD 4xFloat Instructions (.4S) ──────────────────────

void Arm64FpSimdEmitter::EmitVadd4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FADD V<d>.4S, V<n>.4S, V<m>.4S -> 0x4E20D400
	uint32_t inst = 0x4E20D400u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVsub4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FSUB V<d>.4S, V<n>.4S, V<m>.4S -> 0x4EA0D400
	uint32_t inst = 0x4EA0D400u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVmul4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FMUL V<d>.4S, V<n>.4S, V<m>.4S -> 0x6E20DC00
	uint32_t inst = 0x6E20DC00u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitVdiv4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm) {
	// FDIV V<d>.4S, V<n>.4S, V<m>.4S -> 0x6E20FC00
	uint32_t inst = 0x6E20FC00u |
	                ((static_cast<uint32_t>(rm) & 0x1Fu) << 16) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rd) & 0x1Fu);
	m_emitter.Emit32(inst);
}

// ─── Scalar FP Load & Store Instructions ─────────────────────────────────────

void Arm64FpSimdEmitter::EmitLdrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	// LDR S<t>, [X<n>, #offset] -> 0xBD400000
	uint32_t imm12 = (static_cast<uint32_t>(offset / 4) & 0xFFFu);
	uint32_t inst = 0xBD400000u |
	                (imm12 << 10) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

void Arm64FpSimdEmitter::EmitStrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset) {
	// STR S<t>, [X<n>, #offset] -> 0xBD000000
	uint32_t imm12 = (static_cast<uint32_t>(offset / 4) & 0xFFFu);
	uint32_t inst = 0xBD000000u |
	                (imm12 << 10) |
	                ((static_cast<uint32_t>(rn) & 0x1Fu) << 5) |
	                (static_cast<uint32_t>(rt) & 0x1Fu);
	m_emitter.Emit32(inst);
}

} // namespace Loader::Recompiler
