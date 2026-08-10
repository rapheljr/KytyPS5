// avx2VectorEngine.h
//
// 256-bit AVX / AVX2 / FMA3 Vector Extension Engine for KytyPS5 ARM64 Recompiler.
// Handles 256-bit YMM register state, VEX-prefix vector lowering to paired ARM64 NEON registers,
// and high-throughput vector math simulation (AVX Float, AVX2 Int, and FMA3).

#ifndef LOADER_RECOMPILER_AVX2_VECTOR_ENGINE_H
#define LOADER_RECOMPILER_AVX2_VECTOR_ENGINE_H

#include "common/common.h"
#include "loader/recompiler/arm64FpSimdEmitter.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Loader::Recompiler {

// ─── 256-bit YMM Register Representation ─────────────────────────────────────

struct alignas(32) YmmVector256 {
	union {
		float    f32[8];
		double   f64[4];
		int32_t  i32[8];
		uint32_t u32[8];
		int64_t  i64[4];
		uint64_t u64[4];
		uint8_t  u8[32];
	};

	constexpr YmmVector256() : u64{0, 0, 0, 0} {}

	bool operator==(const YmmVector256& other) const noexcept {
		return std::memcmp(u8, other.u8, 32) == 0;
	}

	bool operator!=(const YmmVector256& other) const noexcept {
		return !(*this == other);
	}
};

enum class AvxOpcode256 : uint16_t {
	Vaddps,
	Vsubps,
	Vmulps,
	Vdivps,
	Vmaxps,
	Vminps,
	Vfmadd132ps,
	Vfmadd213ps,
	Vfmadd231ps,
	Vfmsub231ps,
	Vpaddd,
	Vpsubd,
	Vpand,
	Vpor,
	Vpxor,
	Vpandn,
	Vpcmpeqd,
	Vpcmpgtd,
	Vbroadcastss,
	Vpermd
};

struct AvxEngineStats {
	uint64_t total_avx_ops_executed = 0;
	uint64_t total_fma_ops_executed = 0;
	uint64_t total_avx2_int_ops_executed = 0;
};

// ─── AVX2 Vector Engine ──────────────────────────────────────────────────────

class Avx2VectorEngine {
public:
	Avx2VectorEngine();
	~Avx2VectorEngine() = default;

	KYTY_CLASS_NO_COPY(Avx2VectorEngine);

	/// Execute 256-bit AVX / AVX2 / FMA3 vector operation in software simulation
	YmmVector256 Execute(AvxOpcode256 op, const YmmVector256& src1, const YmmVector256& src2, const YmmVector256& src3 = {});

	/// Lower a 256-bit AVX instruction to paired ARM64 NEON instructions (Low 128-bit + High 128-bit)
	bool LowerToArm64Neon(Arm64FpSimdEmitter& emitter, AvxOpcode256 op,
	                      Arm64FpReg dst_lo, Arm64FpReg dst_hi,
	                      Arm64FpReg src1_lo, Arm64FpReg src1_hi,
	                      Arm64FpReg src2_lo, Arm64FpReg src2_hi,
	                      Arm64FpReg src3_lo = Arm64FpReg::V0, Arm64FpReg src3_hi = Arm64FpReg::V0);

	[[nodiscard]] const AvxEngineStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = {}; }

	// Utility vector constructors
	static YmmVector256 FromFloat8(float f0, float f1, float f2, float f3, float f4, float f5, float f6, float f7);
	static YmmVector256 FromInt8(int32_t i0, int32_t i1, int32_t i2, int32_t i3, int32_t i4, int32_t i5, int32_t i6, int32_t i7);
	static YmmVector256 BroadcastFloat(float value);

private:
	AvxEngineStats m_stats{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_AVX2_VECTOR_ENGINE_H
