// arm64FpSimdEmitter.h
//
// ARM64 Floating-Point Scalar & 128-Bit NEON Vector SIMD Instruction Emitter.

#ifndef LOADER_RECOMPILER_ARM64_FP_SIMD_EMITTER_H
#define LOADER_RECOMPILER_ARM64_FP_SIMD_EMITTER_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"

#include <cstdint>

namespace Loader::Recompiler {

enum class Arm64FpReg : uint8_t {
	S0 = 0, S1, S2, S3, S4, S5, S6, S7,
	D0 = 0, D1, D2, D3, D4, D5, D6, D7,
	V0 = 0, V1, V2, V3, V4, V5, V6, V7,
	V8, V9, V10, V11, V12, V13, V14, V15
};

class Arm64FpSimdEmitter {
public:
	explicit Arm64FpSimdEmitter(Arm64Emitter& emitter) : m_emitter(emitter) {}
	~Arm64FpSimdEmitter() = default;

	KYTY_CLASS_NO_COPY(Arm64FpSimdEmitter);

	// Scalar FP Single-Precision (S-Regs)
	void EmitFaddS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitFsubS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitFmulS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitFdivS(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);

	// 128-Bit NEON Vector SIMD 4xFloat (V-Regs .4S)
	void EmitVadd4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitVsub4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitVmul4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);
	void EmitVdiv4S(Arm64FpReg rd, Arm64FpReg rn, Arm64FpReg rm);

	// Scalar FP Load & Store
	void EmitLdrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset);
	void EmitStrS(Arm64FpReg rt, Arm64Reg rn, int32_t offset);

private:
	Arm64Emitter& m_emitter;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_FP_SIMD_EMITTER_H
