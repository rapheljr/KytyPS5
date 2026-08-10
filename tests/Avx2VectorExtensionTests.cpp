// Avx2VectorExtensionTests.cpp
//
// Unit & Integration Tests for 256-bit AVX / AVX2 / FMA3 Vector Engine.

#include "loader/recompiler/avx2VectorEngine.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static void TestAvxFloatArithmetic() {
	std::printf("[TEST] AvxFloatArithmetic\n");

	Avx2VectorEngine engine;

	auto a = Avx2VectorEngine::FromFloat8(1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f);
	auto b = Avx2VectorEngine::FromFloat8(10.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f);

	// VADDPS
	auto sum = engine.Execute(AvxOpcode256::Vaddps, a, b);
	if (std::abs(sum.f32[0] - 11.0f) > 1e-4f || std::abs(sum.f32[7] - 88.0f) > 1e-4f) {
		std::fprintf(stderr, "FAIL: Vaddps result mismatch: %f, %f\n", sum.f32[0], sum.f32[7]);
		std::exit(1);
	}

	// VMULPS
	auto prod = engine.Execute(AvxOpcode256::Vmulps, a, b);
	if (std::abs(prod.f32[3] - 160.0f) > 1e-4f || std::abs(prod.f32[6] - 490.0f) > 1e-4f) {
		std::fprintf(stderr, "FAIL: Vmulps result mismatch: %f, %f\n", prod.f32[3], prod.f32[6]);
		std::exit(1);
	}

	// VDIVPS
	auto div = engine.Execute(AvxOpcode256::Vdivps, b, a);
	if (std::abs(div.f32[0] - 10.0f) > 1e-4f || std::abs(div.f32[7] - 10.0f) > 1e-4f) {
		std::fprintf(stderr, "FAIL: Vdivps result mismatch: %f, %f\n", div.f32[0], div.f32[7]);
		std::exit(1);
	}

	std::printf("  [ OK ] AvxFloatArithmetic\n");
}

static void TestFma3FusedMultiplyAdd() {
	std::printf("[TEST] Fma3FusedMultiplyAdd\n");

	Avx2VectorEngine engine;

	auto a = Avx2VectorEngine::BroadcastFloat(2.0f);
	auto b = Avx2VectorEngine::BroadcastFloat(3.0f);
	auto c = Avx2VectorEngine::BroadcastFloat(4.0f);

	// VFMADD213PS: a * b + c = 2 * 3 + 4 = 10
	auto res213 = engine.Execute(AvxOpcode256::Vfmadd213ps, a, b, c);
	for (int i = 0; i < 8; ++i) {
		if (std::abs(res213.f32[i] - 10.0f) > 1e-4f) {
			std::fprintf(stderr, "FAIL: Vfmadd213ps result[%d] mismatch: %f\n", i, res213.f32[i]);
			std::exit(1);
		}
	}

	// VFMADD231PS: b * c + a = 3 * 4 + 2 = 14
	auto res231 = engine.Execute(AvxOpcode256::Vfmadd231ps, a, b, c);
	for (int i = 0; i < 8; ++i) {
		if (std::abs(res231.f32[i] - 14.0f) > 1e-4f) {
			std::fprintf(stderr, "FAIL: Vfmadd231ps result[%d] mismatch: %f\n", i, res231.f32[i]);
			std::exit(1);
		}
	}

	std::printf("  [ OK ] Fma3FusedMultiplyAdd\n");
}

static void TestAvx2IntegerAndPermute() {
	std::printf("[TEST] Avx2IntegerAndPermute\n");

	Avx2VectorEngine engine;

	auto a = Avx2VectorEngine::FromInt8(10, 20, 30, 40, 50, 60, 70, 80);
	auto b = Avx2VectorEngine::FromInt8(1, 2, 3, 4, 5, 6, 7, 8);

	// VPADDD
	auto sum = engine.Execute(AvxOpcode256::Vpaddd, a, b);
	if (sum.i32[0] != 11 || sum.i32[7] != 88) {
		std::fprintf(stderr, "FAIL: Vpaddd mismatch\n");
		std::exit(1);
	}

	// VPCMPEQD
	auto eq = engine.Execute(AvxOpcode256::Vpcmpeqd, a, a);
	for (int i = 0; i < 8; ++i) {
		if (eq.u32[i] != 0xFFFFFFFFu) {
			std::fprintf(stderr, "FAIL: Vpcmpeqd mismatch at index %d\n", i);
			std::exit(1);
		}
	}

	// VBROADCASTSS
	auto single = Avx2VectorEngine::FromFloat8(42.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	auto bcast = engine.Execute(AvxOpcode256::Vbroadcastss, single, {});
	for (int i = 0; i < 8; ++i) {
		if (std::abs(bcast.f32[i] - 42.0f) > 1e-4f) {
			std::fprintf(stderr, "FAIL: Vbroadcastss mismatch at index %d\n", i);
			std::exit(1);
		}
	}

	// VPERMD
	// Permute table indices: reverse order [7, 6, 5, 4, 3, 2, 1, 0]
	auto perm_idx = Avx2VectorEngine::FromInt8(7, 6, 5, 4, 3, 2, 1, 0);
	auto permuted = engine.Execute(AvxOpcode256::Vpermd, perm_idx, a);
	if (permuted.i32[0] != 80 || permuted.i32[7] != 10) {
		std::fprintf(stderr, "FAIL: Vpermd reverse failed: [0]=%d, [7]=%d\n", permuted.i32[0], permuted.i32[7]);
		std::exit(1);
	}

	std::printf("  [ OK ] Avx2IntegerAndPermute\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — AVX / AVX2 / FMA3 256-Bit Vector Engine Test Suite\n");
	std::printf("================================================================================\n");

	TestAvxFloatArithmetic();
	TestFma3FusedMultiplyAdd();
	TestAvx2IntegerAndPermute();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
