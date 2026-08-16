// X86ToArm64RecompilerTests.cpp
//
// Unit, native JIT execution, multi-threaded stress, and benchmark test suite for Phase M:
// Native x86-64 to ARM64 Dynamic Recompiler.

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/x86BlockCache.h"
#include "loader/recompiler/x86Decoder.h"
#include "loader/recompiler/x86RecompilerIR.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

// ─── 1. Frontend x86-64 Instruction Decoder Test ───────────────────────────────

void TestX86Decoder() {
	std::printf("  [Test 1] Frontend x86-64 Instruction Decoder...\n");

	// 1. mov rax, 0x1122334455667788 (REX.W + 0xB8 + imm64) -> 48 B8 88 77 66 55 44 33 22 11
	uint8_t mov_rax_imm[] = {0x48, 0xB8, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};
	DecodedX86Instruction d1 = X86Decoder::DecodeInstruction(mov_rax_imm, sizeof(mov_rax_imm), 0x1000);
	Check(d1.opcode == X86Opcode::Mov, "Mov opcode decode failed");
	Check(d1.dst.reg == X86Reg::RAX, "Dst reg RAX decode failed");
	Check(d1.src.imm == static_cast<int64_t>(0x1122334455667788ULL), "Imm64 value decode failed");
	Check(d1.length == 10, "Instruction length decode failed");

	// 2. add rax, rbx (REX.W 0x48 + 0x01 + ModRM 0xD8) -> 48 01 D8
	uint8_t add_rax_rbx[] = {0x48, 0x01, 0xD8};
	DecodedX86Instruction d2 = X86Decoder::DecodeInstruction(add_rax_rbx, sizeof(add_rax_rbx), 0x100A);
	Check(d2.opcode == X86Opcode::Add, "Add opcode decode failed");
	Check(d2.dst.reg == X86Reg::RAX && d2.src.reg == X86Reg::RBX, "Add registers decode failed");

	// 3. ret (0xC3)
	uint8_t ret_code[] = {0xC3};
	DecodedX86Instruction d3 = X86Decoder::DecodeInstruction(ret_code, sizeof(ret_code), 0x100D);
	Check(d3.opcode == X86Opcode::Ret, "Ret opcode decode failed");

	std::printf("  [OK] Test 1: Frontend x86-64 Instruction Decoder\n");
}

// ─── 2. ARM64 Backend Emitter Test ───────────────────────────────────────────

void TestArm64EmitterEncoding() {
	std::printf("  [Test 2] ARM64 Backend Instruction Emitter Encoding...\n");

	Arm64Emitter emitter;
	emitter.EmitMovImm64(Arm64Reg::X0, 0x42);
	emitter.EmitAddImm(Arm64Reg::X0, Arm64Reg::X0, 10);
	emitter.EmitRet();

	Check(emitter.GetCode().size() == 3, "Emitted ARM64 instruction word count mismatch");

	std::printf("  [OK] Test 2: ARM64 Backend Instruction Emitter Encoding\n");
}

// ─── 3. Lock-Free Code Cache & Hash Lookup Test ────────────────────────────────

void TestLockFreeCodeCache() {
	std::printf("  [Test 3] Lock-Free Guest RIP Code Cache & Hash Lookup...\n");

	X86BlockCache cache(1024);
	auto dummy_func = reinterpret_cast<CompiledBlockFunc>(0xDEADBEEF);

	cache.Insert(0x400000, dummy_func);
	CompiledBlockFunc found = cache.Lookup(0x400000);
	Check(found == dummy_func, "Cache lookup expected hit failed");

	CompiledBlockFunc miss = cache.Lookup(0x500000);
	Check(miss == nullptr, "Cache lookup expected miss failed");

	cache.Invalidate(0x400000);
	Check(cache.Lookup(0x400000) == nullptr, "Invalidated block lookup should return nullptr");

	std::printf("  [OK] Test 3: Lock-Free Guest RIP Code Cache & Hash Lookup\n");
}

// ─── 4. Native JIT Execution & Runtime Bridge Test ────────────────────────────

void TestNativeJitExecution() {
	std::printf("  [Test 4] Native JIT Code Execution & Runtime Bridge...\n");

	X86RuntimeBridge bridge;

	// Guest x86-64 snippet:
	// 1. mov rax, 0x10 (48 B8 10 00 00 00 00 00 00 00)
	// 2. mov rbx, 0x20 (48 BB 20 00 00 00 00 00 00 00)
	// 3. add rax, rbx (48 01 D8)
	// 4. ret          (C3)
	uint8_t x86_code[] = {
		0x48, 0xB8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rax, 16
		0x48, 0xBB, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // mov rbx, 32
		0x48, 0x01, 0xD8,                                           // add rax, rbx
		0xC3                                                        // ret
	};

	GuestCpuContext ctx{};
	ctx.rip = 0x400000;

	bool success = bridge.ExecuteBlock(ctx, x86_code, sizeof(x86_code));
	Check(success, "JIT block execution failed");

	std::printf("  [OK] Test 4: Native JIT Code Execution & Runtime Bridge\n");
}

// ─── 5. Multi-Threaded Compilation Stress Test ───────────────────────────────

void TestMultiThreadedCompilation() {
	std::printf("  [Test 5] Multi-Threaded Compilation Stress Test (8 threads)...\n");

	constexpr int kNumThreads = 8;
	constexpr int kBlocksPerThread = 500;

	std::vector<std::thread> threads;
	X86RuntimeBridge bridge;

	uint8_t x86_code[] = {
		0x48, 0xB8, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0xC3
	};

	for (int i = 0; i < kNumThreads; ++i) {
		threads.emplace_back([i, &bridge, &x86_code]() {
			for (int j = 0; j < kBlocksPerThread; ++j) {
				uint64_t rip = 0x1000000 + (i * kBlocksPerThread + j) * 0x10;
				CompiledBlockFunc f = bridge.CompileAndCacheBlock(x86_code, sizeof(x86_code), rip);
				Check(f != nullptr, "Concurrent compilation failed");
			}
		});
	}

	for (auto& t : threads) {
		t.join();
	}

	Check(bridge.GetBlockCache().GetStats().blocks_compiled >= static_cast<size_t>(kNumThreads * kBlocksPerThread), "Compiled count stat mismatch");

	std::printf("  [OK] Test 5: Multi-Threaded Compilation Stress Test\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkX86ToArm64Recompiler() {
	std::printf("\n--- Phase M Benchmarks ---\n");

	constexpr int kBenchBlocks = 10000;
	X86RuntimeBridge bridge;

	uint8_t x86_code[] = {
		0x48, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x48, 0xBB, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x48, 0x01, 0xD8,
		0xC3
	};

	// 1. Translation Throughput Benchmark
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kBenchBlocks; ++i) {
		uint64_t rip = 0x2000000 + i * 0x20;
		bridge.CompileAndCacheBlock(x86_code, sizeof(x86_code), rip);
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double total_dt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	double per_block_us = (total_dt_ms * 1000.0) / kBenchBlocks;
	double block_rate = kBenchBlocks / (total_dt_ms / 1000.0);

	std::printf("  [Bench] Recompiler Translation Latency: %.2f us / block\n", per_block_us);
	std::printf("  [Bench] Translation Throughput: %.2f blocks / sec (Tested %d blocks)\n", block_rate, kBenchBlocks);

	// 2. Lock-Free Block Cache Lookup Latency Benchmark
	constexpr int kLookups = 1000000;
	t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kLookups; ++i) {
		uint64_t rip = 0x2000000 + (i % kBenchBlocks) * 0x20;
		bridge.GetBlockCache().Lookup(rip);
	}
	t1 = std::chrono::high_resolution_clock::now();

	double lookup_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kLookups;
	double lookup_rate = kLookups / (std::chrono::duration<double>(t1 - t0).count());

	std::printf("  [Bench] Lock-Free Cache Lookup Latency: %.2f ns / lookup (Throughput: %.2f M lookups/sec)\n",
	           lookup_dt_ns, lookup_rate / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase M: Native x86-64 to ARM64 Recompiler \n");
	std::printf("====================================================\n\n");

	TestX86Decoder();
	TestArm64EmitterEncoding();
	TestLockFreeCodeCache();
	TestNativeJitExecution();
	TestMultiThreadedCompilation();

	BenchmarkX86ToArm64Recompiler();

	std::printf("\nX86ToArm64RecompilerTests: ALL PASSED\n");
	return 0;
}
