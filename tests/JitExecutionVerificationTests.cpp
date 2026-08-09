// JitExecutionVerificationTests.cpp
//
// Direct JIT Execution Verification Test Suite on Native Apple Silicon (ARM64).
// Verifies real machine code execution, AAPCS64 register bridge, GPR arithmetic,
// 128-bit SSE/AVX vector SIMD (4-wide float) operations, and guest context mutation.

#include "loader/recompiler/x86RuntimeBridge.h"
#include "loader/recompiler/x86ToIRLowering.h"
#include "loader/recompiler/irOptimizationPasses.h"
#include "loader/recompiler/arm64IRCodegen.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cmath>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

static void SetXmmFloats(GuestCpuContext& c, int reg, float f0, float f1, float f2, float f3) {
	float f[4] = {f0, f1, f2, f3};
	std::memcpy(&c.xmm[reg], f, sizeof(f));
}

static float GetXmmFloat(const GuestCpuContext& c, int reg, int idx) {
	float f[4];
	std::memcpy(f, &c.xmm[reg], sizeof(f));
	return f[idx];
}

void TestDirectExecution_MovAndAdd() {
	std::printf("[TEST] JitDirectExecution_MovAndAdd\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400000;

	// Machine code:
	//   mov eax, 42       (B8 2A 00 00 00)
	//   add eax, 58       (05 3A 00 00 00)
	//   ret               (C3)
	const uint8_t code[] = {
		0xB8, 0x2A, 0x00, 0x00, 0x00,
		0x05, 0x3A, 0x00, 0x00, 0x00,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed");
	Check(ctx.rax == 100, "ctx.rax != 100 after mov eax, 42; add eax, 58");

	std::printf("  RAX Result = %llu (Expected 100)\n", (unsigned long long)ctx.rax);
	std::printf("  [ OK ] JitDirectExecution_MovAndAdd\n");
}

void TestDirectExecution_TwoRegisters() {
	std::printf("[TEST] JitDirectExecution_TwoRegisters\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400100;

	// Machine code:
	//   mov rax, 25       (48 C7 C0 19 00 00 00)
	//   mov rbx, 17       (48 C7 C3 11 00 00 00)
	//   add rax, rbx      (48 01 D8)
	//   ret               (C3)
	const uint8_t code[] = {
		0x48, 0xC7, 0xC0, 0x19, 0x00, 0x00, 0x00,
		0x48, 0xC7, 0xC3, 0x11, 0x00, 0x00, 0x00,
		0x48, 0x01, 0xD8,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed");
	Check(ctx.rax == 42, "ctx.rax != 42 after add rax, rbx");
	Check(ctx.rbx == 17, "ctx.rbx != 17 after mov rbx, 17");

	std::printf("  RAX Result = %llu (Expected 42), RBX = %llu (Expected 17)\n",
	            (unsigned long long)ctx.rax, (unsigned long long)ctx.rbx);
	std::printf("  [ OK ] JitDirectExecution_TwoRegisters\n");
}

void TestDirectExecution_SubAndXor() {
	std::printf("[TEST] JitDirectExecution_SubAndXor\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400200;

	// Machine code:
	//   mov rax, 100      (48 C7 C0 64 00 00 00)
	//   sub rax, 25       (48 83 E8 19)
	//   xor rax, 15       (48 83 F0 0F)
	//   ret               (C3)
	const uint8_t code[] = {
		0x48, 0xC7, 0xC0, 0x64, 0x00, 0x00, 0x00,
		0x48, 0x83, 0xE8, 0x19,
		0x48, 0x83, 0xF0, 0x0F,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed");
	// 100 - 25 = 75. 75 ^ 15 = 68.
	Check(ctx.rax == 68, "ctx.rax != 68 after sub and xor");

	std::printf("  RAX Result = %llu (Expected 68)\n", (unsigned long long)ctx.rax);
	std::printf("  [ OK ] JitDirectExecution_SubAndXor\n");
}

void TestDirectExecution_LogicalAndShift() {
	std::printf("[TEST] JitDirectExecution_LogicalAndShift\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400300;

	// Machine code:
	//   mov rax, 255      (48 C7 C0 FF 00 00 00)
	//   and rax, 15       (48 83 E0 0F)
	//   mov rcx, 2        (48 C7 C1 02 00 00 00)
	//   shl rax, cl       (48 D3 E0)
	//   ret               (C3)
	const uint8_t code[] = {
		0x48, 0xC7, 0xC0, 0xFF, 0x00, 0x00, 0x00,
		0x48, 0x83, 0xE0, 0x0F,
		0x48, 0xC7, 0xC1, 0x02, 0x00, 0x00, 0x00,
		0x48, 0xD3, 0xE0,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed");
	// (255 & 15) = 15. 15 << 2 = 60.
	Check(ctx.rax == 60, "ctx.rax != 60 after and and shl");

	std::printf("  RAX Result = %llu (Expected 60)\n", (unsigned long long)ctx.rax);
	std::printf("  [ OK ] JitDirectExecution_LogicalAndShift\n");
}

void TestDirectExecution_ComplexArithmetic() {
	std::printf("[TEST] JitDirectExecution_ComplexArithmetic\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400400;

	// Machine code:
	//   mov rax, 50       (48 C7 C0 32 00 00 00)
	//   mov rbx, 10       (48 C7 C3 0A 00 00 00)
	//   add rax, rbx      (48 01 D8)              ; rax = 60
	//   mov rcx, 15       (48 C7 C1 0F 00 00 00)
	//   sub rax, rcx      (48 29 C8)              ; rax = 45
	//   mov rdx, 3        (48 C7 C2 03 00 00 00)
	//   imul rax, rdx     (48 0F AF C2)           ; rax = 135
	//   ret               (C3)
	const uint8_t code[] = {
		0x48, 0xC7, 0xC0, 0x32, 0x00, 0x00, 0x00,
		0x48, 0xC7, 0xC3, 0x0A, 0x00, 0x00, 0x00,
		0x48, 0x01, 0xD8,
		0x48, 0xC7, 0xC1, 0x0F, 0x00, 0x00, 0x00,
		0x48, 0x29, 0xC8,
		0x48, 0xC7, 0xC2, 0x03, 0x00, 0x00, 0x00,
		0x48, 0x0F, 0xAF, 0xC2,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed");
	Check(ctx.rax == 135, "ctx.rax != 135 after ((50 + 10) - 15) * 3");
	Check(ctx.rbx == 10, "ctx.rbx != 10");
	Check(ctx.rcx == 15, "ctx.rcx != 15");
	Check(ctx.rdx == 3, "ctx.rdx != 3");

	std::printf("  RAX = %llu, RBX = %llu, RCX = %llu, RDX = %llu\n",
	            (unsigned long long)ctx.rax, (unsigned long long)ctx.rbx,
	            (unsigned long long)ctx.rcx, (unsigned long long)ctx.rdx);
	std::printf("  [ OK ] JitDirectExecution_ComplexArithmetic\n");
}

void TestDirectExecution_ContextPersistence() {
	std::printf("[TEST] JitDirectExecution_ContextPersistence\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400500;
	ctx.rax = 1000;
	ctx.r8 = 250;

	// Block 1: add rax, r8 (4C 01 C0), ret (C3) -> rax = 1250
	const uint8_t code1[] = {
		0x4C, 0x01, 0xC0,
		0xC3
	};

	bool ok1 = bridge.ExecuteBlock(ctx, code1, sizeof(code1));
	Check(ok1, "Block 1 execution failed");
	Check(ctx.rax == 1250, "ctx.rax != 1250 after Block 1");

	// Block 2: add rax, 750 (48 05 EE 02 00 00), ret (C3) -> rax = 2000
	ctx.rip = 0x400600;
	const uint8_t code2[] = {
		0x48, 0x05, 0xEE, 0x02, 0x00, 0x00,
		0xC3
	};

	bool ok2 = bridge.ExecuteBlock(ctx, code2, sizeof(code2));
	Check(ok2, "Block 2 execution failed");
	Check(ctx.rax == 2000, "ctx.rax != 2000 after Block 2");

	std::printf("  Accumulated RAX Across Blocks = %llu (Expected 2000)\n", (unsigned long long)ctx.rax);
	std::printf("  [ OK ] JitDirectExecution_ContextPersistence\n");
}

// ─── SSE / AVX 128-Bit Vector SIMD Tests ─────────────────────────────────────

void TestDirectExecution_VecAdd4S() {
	std::printf("[TEST] JitDirectExecution_VecAdd4S (ADDPS XMM0, XMM1)\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400700;

	SetXmmFloats(ctx, 0, 1.0f, 2.0f, 3.0f, 4.0f);
	SetXmmFloats(ctx, 1, 10.0f, 20.0f, 30.0f, 40.0f);

	// Machine code:
	//   addps xmm0, xmm1  (0F 58 C1)
	//   ret               (C3)
	const uint8_t code[] = {
		0x0F, 0x58, 0xC1,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed for ADDPS");

	float r0 = GetXmmFloat(ctx, 0, 0);
	float r1 = GetXmmFloat(ctx, 0, 1);
	float r2 = GetXmmFloat(ctx, 0, 2);
	float r3 = GetXmmFloat(ctx, 0, 3);

	Check(r0 == 11.0f && r1 == 22.0f && r2 == 33.0f && r3 == 44.0f, "ADDPS result mismatch");

	std::printf("  XMM0 Result = {%.1f, %.1f, %.1f, %.1f} (Expected {11.0, 22.0, 33.0, 44.0})\n", r0, r1, r2, r3);
	std::printf("  [ OK ] JitDirectExecution_VecAdd4S\n");
}

void TestDirectExecution_VecMulAndSub4S() {
	std::printf("[TEST] JitDirectExecution_VecMulAndSub4S (MULPS & SUBPS)\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400800;

	SetXmmFloats(ctx, 0, 2.0f, 3.0f, 4.0f, 5.0f);
	SetXmmFloats(ctx, 1, 10.0f, 10.0f, 10.0f, 10.0f);
	SetXmmFloats(ctx, 2, 1.0f, 2.0f, 3.0f, 4.0f);

	// Machine code:
	//   mulps xmm0, xmm1  (0F 59 C1)  ; xmm0 = {20, 30, 40, 50}
	//   subps xmm0, xmm2  (0F 5C C2)  ; xmm0 = {19, 28, 37, 46}
	//   ret               (C3)
	const uint8_t code[] = {
		0x0F, 0x59, 0xC1,
		0x0F, 0x5C, 0xC2,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed for MULPS/SUBPS");

	float r0 = GetXmmFloat(ctx, 0, 0);
	float r1 = GetXmmFloat(ctx, 0, 1);
	float r2 = GetXmmFloat(ctx, 0, 2);
	float r3 = GetXmmFloat(ctx, 0, 3);

	Check(r0 == 19.0f && r1 == 28.0f && r2 == 37.0f && r3 == 46.0f, "MULPS/SUBPS result mismatch");

	std::printf("  XMM0 Result = {%.1f, %.1f, %.1f, %.1f} (Expected {19.0, 28.0, 37.0, 46.0})\n", r0, r1, r2, r3);
	std::printf("  [ OK ] JitDirectExecution_VecMulAndSub4S\n");
}

void TestDirectExecution_VecDivAndSqrt4S() {
	std::printf("[TEST] JitDirectExecution_VecDivAndSqrt4S (DIVPS & SQRTPS)\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400900;

	SetXmmFloats(ctx, 0, 64.0f, 144.0f, 256.0f, 400.0f);
	SetXmmFloats(ctx, 1, 4.0f, 4.0f, 4.0f, 4.0f);

	// Machine code:
	//   divps  xmm0, xmm1  (0F 5E C1)  ; xmm0 = {16, 36, 64, 100}
	//   sqrtps xmm0, xmm0  (0F 51 C0)  ; xmm0 = {4, 6, 8, 10}
	//   ret                (C3)
	const uint8_t code[] = {
		0x0F, 0x5E, 0xC1,
		0x0F, 0x51, 0xC0,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed for DIVPS/SQRTPS");

	float r0 = GetXmmFloat(ctx, 0, 0);
	float r1 = GetXmmFloat(ctx, 0, 1);
	float r2 = GetXmmFloat(ctx, 0, 2);
	float r3 = GetXmmFloat(ctx, 0, 3);

	Check(std::fabs(r0 - 4.0f) < 0.001f &&
	      std::fabs(r1 - 6.0f) < 0.001f &&
	      std::fabs(r2 - 8.0f) < 0.001f &&
	      std::fabs(r3 - 10.0f) < 0.001f, "DIVPS/SQRTPS result mismatch");

	std::printf("  XMM0 Result = {%.1f, %.1f, %.1f, %.1f} (Expected {4.0, 6.0, 8.0, 10.0})\n", r0, r1, r2, r3);
	std::printf("  [ OK ] JitDirectExecution_VecDivAndSqrt4S\n");
}

void TestDirectExecution_VecBitwiseLogic() {
	std::printf("[TEST] JitDirectExecution_VecBitwiseLogic (XORPS & ANDPS)\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400A00;

	uint32_t u0[4] = { 0xAAAAAAAA, 0x55555555, 0x0F0F0F0F, 0xF0F0F0F0 };
	uint32_t u1[4] = { 0xFFFFFFFF, 0x00000000, 0xFF00FF00, 0x00FF00FF };
	uint32_t u2[4] = { 0x0F0F0F0F, 0xF0F0F0F0, 0xAAAAAAAA, 0x55555555 };

	std::memcpy(&ctx.xmm[0], u0, sizeof(u0));
	std::memcpy(&ctx.xmm[1], u1, sizeof(u1));
	std::memcpy(&ctx.xmm[2], u2, sizeof(u2));

	// Machine code:
	//   xorps xmm0, xmm1  (0F 57 C1)
	//   andps xmm0, xmm2  (0F 54 C2)
	//   ret               (C3)
	const uint8_t code[] = {
		0x0F, 0x57, 0xC1,
		0x0F, 0x54, 0xC2,
		0xC3
	};

	bool ok = bridge.ExecuteBlock(ctx, code, sizeof(code));
	Check(ok, "ExecuteBlock failed for XORPS/ANDPS");

	uint32_t res[4];
	std::memcpy(res, &ctx.xmm[0], sizeof(res));

	for (int i = 0; i < 4; ++i) {
		uint32_t expected = (u0[i] ^ u1[i]) & u2[i];
		Check(res[i] == expected, "Bitwise logic result mismatch");
	}

	std::printf("  [ OK ] JitDirectExecution_VecBitwiseLogic\n");
}

void TestDirectExecution_VecContextPersistence() {
	std::printf("[TEST] JitDirectExecution_VecContextPersistence (Multi-Block XMM State)\n");

	X86RuntimeBridge bridge;
	GuestCpuContext ctx{};
	ctx.rip = 0x400B00;

	SetXmmFloats(ctx, 0, 1.0f, 2.0f, 3.0f, 4.0f);
	SetXmmFloats(ctx, 1, 5.0f, 5.0f, 5.0f, 5.0f);
	SetXmmFloats(ctx, 2, 2.0f, 2.0f, 2.0f, 2.0f);

	// Block 1: addps xmm0, xmm1 (0F 58 C1), ret (C3) -> xmm0 = {6, 7, 8, 9}
	const uint8_t code1[] = {
		0x0F, 0x58, 0xC1,
		0xC3
	};

	bool ok1 = bridge.ExecuteBlock(ctx, code1, sizeof(code1));
	Check(ok1, "Block 1 execution failed");

	// Block 2: mulps xmm0, xmm2 (0F 59 C2), ret (C3) -> xmm0 = {12, 14, 16, 18}
	ctx.rip = 0x400C00;
	const uint8_t code2[] = {
		0x0F, 0x59, 0xC2,
		0xC3
	};

	bool ok2 = bridge.ExecuteBlock(ctx, code2, sizeof(code2));
	Check(ok2, "Block 2 execution failed");

	float r0 = GetXmmFloat(ctx, 0, 0);
	float r1 = GetXmmFloat(ctx, 0, 1);
	float r2 = GetXmmFloat(ctx, 0, 2);
	float r3 = GetXmmFloat(ctx, 0, 3);

	Check(r0 == 12.0f && r1 == 14.0f && r2 == 16.0f && r3 == 18.0f, "Multi-block XMM persistence mismatch");

	std::printf("  Accumulated XMM0 = {%.1f, %.1f, %.1f, %.1f} (Expected {12.0, 14.0, 16.0, 18.0})\n", r0, r1, r2, r3);
	std::printf("  [ OK ] JitDirectExecution_VecContextPersistence\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::setbuf(stderr, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Native Apple Silicon JIT Direct Execution Test Suite\n");
	std::printf("================================================================================\n");

	// GPR Tests
	TestDirectExecution_MovAndAdd();
	TestDirectExecution_TwoRegisters();
	TestDirectExecution_SubAndXor();
	TestDirectExecution_LogicalAndShift();
	TestDirectExecution_ComplexArithmetic();
	TestDirectExecution_ContextPersistence();

	// Vector SIMD SSE / AVX Tests
	TestDirectExecution_VecAdd4S();
	TestDirectExecution_VecMulAndSub4S();
	TestDirectExecution_VecDivAndSqrt4S();
	TestDirectExecution_VecBitwiseLogic();
	TestDirectExecution_VecContextPersistence();

	std::printf("================================================================================\n");
	std::printf("  Results: 11 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	std::fflush(stdout);
	return 0;
}
