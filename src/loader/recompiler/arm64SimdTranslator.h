// arm64SimdTranslator.h
//
// Complete x86 SIMD (SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/AVX/AVX2) to ARM64 NEON Translation Engine.

#ifndef LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H
#define LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H

#include "loader/recompiler/arm64FpSimdEmitter.h"
#include "loader/recompiler/x86Decoder.h"

namespace Loader::Recompiler {

enum class SimdInstructionSet : uint8_t {
	SSE,
	SSE2,
	SSE3,
	SSSE3,
	SSE4_1,
	SSE4_2,
	AVX,
	AVX2
};

class Arm64SimdTranslator {
public:
	explicit Arm64SimdTranslator(Arm64FpSimdEmitter& fp_emitter) : m_fp(fp_emitter) {}
	~Arm64SimdTranslator() = default;

	KYTY_CLASS_NO_COPY(Arm64SimdTranslator);

	// Translation Dispatcher
	bool TranslateInstruction(const DecodedX86Instruction& inst, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);

	// Family-Specific Translators
	bool TranslateSSE(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE2(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE41(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE42(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateAVX(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);

private:
	Arm64FpSimdEmitter& m_fp;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H
