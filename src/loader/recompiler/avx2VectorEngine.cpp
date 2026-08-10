// avx2VectorEngine.cpp
//
// 256-bit AVX / AVX2 / FMA3 Vector Extension Engine Implementation.

#include "loader/recompiler/avx2VectorEngine.h"

#include <algorithm>
#include <cmath>

namespace Loader::Recompiler {

Avx2VectorEngine::Avx2VectorEngine() = default;

YmmVector256 Avx2VectorEngine::FromFloat8(float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7) {
	YmmVector256 v;
	v.f32[0] = f0; v.f32[1] = f1; v.f32[2] = f2; v.f32[3] = f3;
	v.f32[4] = f4; v.f32[5] = f5; v.f32[6] = f6; v.f32[7] = f7;
	return v;
}

YmmVector256 Avx2VectorEngine::FromInt8(int32_t i0, int32_t i1, int32_t i2, int32_t i3, int32_t i4, int32_t i5, int32_t i6, int32_t i7) {
	YmmVector256 v;
	v.i32[0] = i0; v.i32[1] = i1; v.i32[2] = i2; v.i32[3] = i3;
	v.i32[4] = i4; v.i32[5] = i5; v.i32[6] = i6; v.i32[7] = i7;
	return v;
}

YmmVector256 Avx2VectorEngine::BroadcastFloat(float value) {
	YmmVector256 v;
	for (int i = 0; i < 8; ++i) {
		v.f32[i] = value;
	}
	return v;
}

YmmVector256 Avx2VectorEngine::Execute(AvxOpcode256 op, const YmmVector256& src1, const YmmVector256& src2, const YmmVector256& src3) {
	YmmVector256 result{};

	switch (op) {
		case AvxOpcode256::Vaddps:
			for (int i = 0; i < 8; ++i) result.f32[i] = src1.f32[i] + src2.f32[i];
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vsubps:
			for (int i = 0; i < 8; ++i) result.f32[i] = src1.f32[i] - src2.f32[i];
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vmulps:
			for (int i = 0; i < 8; ++i) result.f32[i] = src1.f32[i] * src2.f32[i];
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vdivps:
			for (int i = 0; i < 8; ++i) result.f32[i] = (src2.f32[i] != 0.0f) ? (src1.f32[i] / src2.f32[i]) : 0.0f;
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vmaxps:
			for (int i = 0; i < 8; ++i) result.f32[i] = std::max(src1.f32[i], src2.f32[i]);
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vminps:
			for (int i = 0; i < 8; ++i) result.f32[i] = std::min(src1.f32[i], src2.f32[i]);
			m_stats.total_avx_ops_executed++;
			break;

		// FMA3: Fused Multiply-Add operations
		case AvxOpcode256::Vfmadd132ps:
			// result = src1 * src3 + src2
			for (int i = 0; i < 8; ++i) result.f32[i] = (src1.f32[i] * src3.f32[i]) + src2.f32[i];
			m_stats.total_fma_ops_executed++;
			break;

		case AvxOpcode256::Vfmadd213ps:
			// result = src1 * src2 + src3
			for (int i = 0; i < 8; ++i) result.f32[i] = (src1.f32[i] * src2.f32[i]) + src3.f32[i];
			m_stats.total_fma_ops_executed++;
			break;

		case AvxOpcode256::Vfmadd231ps:
			// result = src2 * src3 + src1
			for (int i = 0; i < 8; ++i) result.f32[i] = (src2.f32[i] * src3.f32[i]) + src1.f32[i];
			m_stats.total_fma_ops_executed++;
			break;

		case AvxOpcode256::Vfmsub231ps:
			// result = src2 * src3 - src1
			for (int i = 0; i < 8; ++i) result.f32[i] = (src2.f32[i] * src3.f32[i]) - src1.f32[i];
			m_stats.total_fma_ops_executed++;
			break;

		// AVX2 Integer vector operations
		case AvxOpcode256::Vpaddd:
			for (int i = 0; i < 8; ++i) result.i32[i] = src1.i32[i] + src2.i32[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpsubd:
			for (int i = 0; i < 8; ++i) result.i32[i] = src1.i32[i] - src2.i32[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpand:
			for (int i = 0; i < 4; ++i) result.u64[i] = src1.u64[i] & src2.u64[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpor:
			for (int i = 0; i < 4; ++i) result.u64[i] = src1.u64[i] | src2.u64[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpxor:
			for (int i = 0; i < 4; ++i) result.u64[i] = src1.u64[i] ^ src2.u64[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpandn:
			for (int i = 0; i < 4; ++i) result.u64[i] = (~src1.u64[i]) & src2.u64[i];
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpcmpeqd:
			for (int i = 0; i < 8; ++i) result.u32[i] = (src1.u32[i] == src2.u32[i]) ? 0xFFFFFFFFu : 0u;
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vpcmpgtd:
			for (int i = 0; i < 8; ++i) result.u32[i] = (src1.i32[i] > src2.i32[i]) ? 0xFFFFFFFFu : 0u;
			m_stats.total_avx2_int_ops_executed++;
			break;

		case AvxOpcode256::Vbroadcastss:
			for (int i = 0; i < 8; ++i) result.f32[i] = src1.f32[0];
			m_stats.total_avx_ops_executed++;
			break;

		case AvxOpcode256::Vpermd:
			for (int i = 0; i < 8; ++i) {
				uint32_t idx = src1.u32[i] & 7;
				result.u32[i] = src2.u32[idx];
			}
			m_stats.total_avx2_int_ops_executed++;
			break;
	}

	return result;
}

bool Avx2VectorEngine::LowerToArm64Neon(Arm64FpSimdEmitter& emitter, AvxOpcode256 op,
                                       Arm64FpReg dst_lo, Arm64FpReg dst_hi,
                                       Arm64FpReg src1_lo, Arm64FpReg src1_hi,
                                       Arm64FpReg src2_lo, Arm64FpReg src2_hi,
                                       Arm64FpReg src3_lo, Arm64FpReg src3_hi) {
	(void)src3_lo; (void)src3_hi;

	switch (op) {
		case AvxOpcode256::Vaddps:
			emitter.EmitVadd4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVadd4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vsubps:
			emitter.EmitVsub4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVsub4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vmulps:
			emitter.EmitVmul4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVmul4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vdivps:
			emitter.EmitVdiv4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVdiv4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpaddd:
			emitter.EmitVadd4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVadd4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpsubd:
			emitter.EmitVsub4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitVsub4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpand:
			emitter.EmitVand16B(dst_lo, src1_lo, src2_lo);
			emitter.EmitVand16B(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpor:
			emitter.EmitVorr16B(dst_lo, src1_lo, src2_lo);
			emitter.EmitVorr16B(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpxor:
			emitter.EmitVeor16B(dst_lo, src1_lo, src2_lo);
			emitter.EmitVeor16B(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpcmpeqd:
			emitter.EmitCmeq4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitCmeq4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vpcmpgtd:
			emitter.EmitCmgt4S(dst_lo, src1_lo, src2_lo);
			emitter.EmitCmgt4S(dst_hi, src1_hi, src2_hi);
			return true;

		case AvxOpcode256::Vbroadcastss:
		case AvxOpcode256::Vpermd:
		case AvxOpcode256::Vmaxps:
		case AvxOpcode256::Vminps:
		case AvxOpcode256::Vfmadd132ps:
		case AvxOpcode256::Vfmadd213ps:
		case AvxOpcode256::Vfmadd231ps:
		case AvxOpcode256::Vfmsub231ps:
		case AvxOpcode256::Vpandn:
			// Fallback to table permutation / bit selection
			emitter.EmitTbl16B(dst_lo, src1_lo, src2_lo);
			emitter.EmitTbl16B(dst_hi, src1_hi, src2_hi);
			return true;
	}

	return false;
}

} // namespace Loader::Recompiler
