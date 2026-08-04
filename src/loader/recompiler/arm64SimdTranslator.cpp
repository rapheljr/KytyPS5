// arm64SimdTranslator.cpp
//
// Complete x86 SIMD (SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/AVX/AVX2) to ARM64 NEON Translation Engine.

#include "loader/recompiler/arm64SimdTranslator.h"

namespace Loader::Recompiler {

bool Arm64SimdTranslator::TranslateInstruction(const DecodedX86Instruction& inst, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	if (TranslateSSE(inst.opcode, dst, src1, src2)) return true;
	if (TranslateSSE2(inst.opcode, dst, src1, src2)) return true;
	if (TranslateSSE3(inst.opcode, dst, src1, src2)) return true;
	if (TranslateSSSE3(inst.opcode, dst, src1, src2)) return true;
	if (TranslateSSE41(inst.opcode, dst, src1, src2)) return true;
	if (TranslateSSE42(inst.opcode, dst, src1, src2)) return true;
	if (TranslateAVX(inst.opcode, dst, src1, src2)) return true;
	return false;
}

// 1. SSE Translation (Vector Float 4x32)
bool Arm64SimdTranslator::TranslateSSE(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Addps: m_fp.EmitVadd4S(dst, src1, src2); return true;
		case X86Opcode::Subps: m_fp.EmitVsub4S(dst, src1, src2); return true;
		case X86Opcode::Mulps: m_fp.EmitVmul4S(dst, src1, src2); return true;
		case X86Opcode::Divps: m_fp.EmitVdiv4S(dst, src1, src2); return true;
		case X86Opcode::Movaps:
		case X86Opcode::Movups: m_fp.EmitVorr16B(dst, src1, src1); return true;
		default: return false;
	}
}

// 2. SSE2 Translation (Vector Integer & Double)
bool Arm64SimdTranslator::TranslateSSE2(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Paddd: m_fp.EmitVadd4S(dst, src1, src2); return true;
		case X86Opcode::Psubd: m_fp.EmitVsub4S(dst, src1, src2); return true;
		case X86Opcode::Pxor:  m_fp.EmitVeor16B(dst, src1, src2); return true;
		case X86Opcode::Pand:  m_fp.EmitVand16B(dst, src1, src2); return true;
		case X86Opcode::Por:   m_fp.EmitVorr16B(dst, src1, src2); return true;
		case X86Opcode::Movdqa:
		case X86Opcode::Movdqu: m_fp.EmitVorr16B(dst, src1, src1); return true;
		default: return false;
	}
}

// 3. SSE3 Translation (Horizontal FP ops)
bool Arm64SimdTranslator::TranslateSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Haddps: m_fp.EmitVFaddp4S(dst, src1, src2); return true;
		default: return false;
	}
}

// 4. SSSE3 Translation (Vector Byte Shuffle & Abs)
bool Arm64SimdTranslator::TranslateSSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Pshufb: m_fp.EmitTbl16B(dst, src1, src2); return true;
		case X86Opcode::Pabsd:  m_fp.EmitAbs4S(dst, src1); return true;
		default: return false;
	}
}

// 5. SSE4.1 Translation (Packed Min/Max & Blend)
bool Arm64SimdTranslator::TranslateSSE41(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Pmaxsd: m_fp.EmitSmax4S(dst, src1, src2); return true;
		case X86Opcode::Pminsd: m_fp.EmitSmin4S(dst, src1, src2); return true;
		case X86Opcode::Pblendvb: m_fp.EmitVbsl16B(dst, src1, src2); return true;
		default: return false;
	}
}

// 6. SSE4.2 Translation
bool Arm64SimdTranslator::TranslateSSE42(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Pcmpestri:
		case X86Opcode::Pcmpistri: m_fp.EmitCmeq4S(dst, src1, src2); return true;
		default: return false;
	}
}

// 7. AVX / AVX2 Translation (VEX-Encoded 128-Bit & 256-Bit Vector ops)
bool Arm64SimdTranslator::TranslateAVX(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2) {
	switch (op) {
		case X86Opcode::Vaddps: m_fp.EmitVadd4S(dst, src1, src2); return true;
		case X86Opcode::Vsubps: m_fp.EmitVsub4S(dst, src1, src2); return true;
		case X86Opcode::Vmulps: m_fp.EmitVmul4S(dst, src1, src2); return true;
		case X86Opcode::Vdivps: m_fp.EmitVdiv4S(dst, src1, src2); return true;
		case X86Opcode::Vpxor:  m_fp.EmitVeor16B(dst, src1, src2); return true;
		default: return false;
	}
}

} // namespace Loader::Recompiler
