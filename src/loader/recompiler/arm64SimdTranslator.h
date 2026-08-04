// arm64SimdTranslator.h
//
// Complete x86 SIMD (MMX/SSE/SSE2/SSE3/SSSE3/SSE4.1/SSE4.2/AVX/AVX2) to ARM64 NEON Translation Engine.

#ifndef LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H
#define LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H

#include "loader/recompiler/arm64FpSimdEmitter.h"
#include "loader/recompiler/x86Decoder.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Loader::Recompiler {

enum class SimdInstructionSet : uint8_t {
	MMX,
	SSE,
	SSE2,
	SSE3,
	SSSE3,
	SSE4_1,
	SSE4_2,
	AVX,
	AVX2
};

struct SimdOpcodeBenchmark {
	std::string        mnemonic;
	SimdInstructionSet set;
	double             latency_ns          = 0.0;
	double             throughput_m_ops_sec = 0.0;
	uint64_t           cpu_cycles          = 0;
};

struct SimdOptimizationSuggestion {
	std::string opcode;
	std::string current_translation;
	std::string recommended_neon_pattern;
	std::string estimated_speedup;
};

class SimdOptimizationAnalyzer {
public:
	static SimdOpcodeBenchmark BenchmarkOpcode(SimdInstructionSet set, const std::string& mnemonic, uint64_t iterations = 1000000);
	static std::vector<SimdOptimizationSuggestion> GenerateOptimizationSuggestions();
};

class Arm64SimdTranslator {
public:
	explicit Arm64SimdTranslator(Arm64FpSimdEmitter& fp_emitter) : m_fp(fp_emitter) {}
	~Arm64SimdTranslator() = default;

	KYTY_CLASS_NO_COPY(Arm64SimdTranslator);

	// Translation Dispatcher
	bool TranslateInstruction(const DecodedX86Instruction& inst, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);

	// Family-Specific Translators
	bool TranslateMMX(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE2(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSSE3(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE41(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateSSE42(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateAVX(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);
	bool TranslateAVX2(X86Opcode op, Arm64FpReg dst, Arm64FpReg src1, Arm64FpReg src2);

private:
	Arm64FpSimdEmitter& m_fp;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_SIMD_TRANSLATOR_H
