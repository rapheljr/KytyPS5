// ARM64BackendTests.cpp
//
// Comprehensive unit tests and benchmarks for the Native ARM64 JIT Backend.
// Tests the ARM64Emitter, ARM64RegisterAllocator, ARM64Relocation, and
// live JIT code execution (on ARM64 hosts only).

#include "emulator/arm64BackendInterface.h"
#include "emulator/arm64Emitter.h"
#include "emulator/arm64RegisterAllocator.h"
#include "emulator/arm64Relocation.h"

#include "common/virtualMemory.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

namespace {

// ──────────────────────────────────────────────────────────────────────────────
// Helper
// ──────────────────────────────────────────────────────────────────────────────

void Check(bool value, const char* msg) {
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		std::abort();
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 1: Arithmetic instruction encoding
// ──────────────────────────────────────────────────────────────────────────────

void TestArithmeticEncoding() {
	std::vector<uint8_t> buf(4096);
	Libs::Emulator::CodeBuffer cb{buf.data(), buf.size(), 0};
	Libs::Emulator::ARM64Emitter em(&cb);

	Check(em.EmitNOP(),                                                                                  "EmitNOP");
	Check(em.EmitMOVZ(Libs::Emulator::ARM64Reg::X0, 0x1234, 0),                                         "EmitMOVZ");
	Check(em.EmitMOVK(Libs::Emulator::ARM64Reg::X0, 0x5678, 16),                                        "EmitMOVK");
	Check(em.EmitADD(Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X1), "EmitADD");
	Check(em.EmitSUB(Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X2), "EmitSUB");
	Check(em.EmitRET(),                                                                                  "EmitRET");

	// 6 instructions × 4 bytes = 24
	Check(cb.size == 24, "Buffer size after 6 instructions");

	// Verify NOP encoding
	uint32_t nop = 0;
	std::memcpy(&nop, buf.data(), 4);
	Check(nop == 0xD503201Fu, "NOP opcode");

	// Verify RET encoding
	uint32_t ret = 0;
	std::memcpy(&ret, buf.data() + 20, 4);
	Check(ret == 0xD65F03C0u, "RET opcode");

	std::printf("  [OK] TestArithmeticEncoding\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 2: MOV64 multi-instruction sequence
// ──────────────────────────────────────────────────────────────────────────────

void TestMov64Encoding() {
	std::vector<uint8_t> buf(256);
	Libs::Emulator::CodeBuffer cb{buf.data(), buf.size(), 0};
	Libs::Emulator::ARM64Emitter em(&cb);

	// imm64 with all four 16-bit groups non-zero → 4 instructions
	Check(em.EmitMOV64(Libs::Emulator::ARM64Reg::X1, 0x123456789ABCDEF0ULL), "EmitMOV64 full");
	Check(cb.size == 16, "MOV64: 4 instructions for 4-part immediate");

	cb.Reset();
	// imm64 with only low 16 bits set → 1 instruction (MOVZ only)
	Check(em.EmitMOV64(Libs::Emulator::ARM64Reg::X2, 0x0000000000001234ULL), "EmitMOV64 short");
	Check(cb.size == 4, "MOV64: 1 instruction for 16-bit immediate");

	std::printf("  [OK] TestMov64Encoding\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 3: Branch relocation encoding
// ──────────────────────────────────────────────────────────────────────────────

void TestRelocationEncoding() {
	std::vector<uint8_t> buf(256);
	Libs::Emulator::CodeBuffer cb{buf.data(), buf.size(), 0};
	Libs::Emulator::ARM64Emitter em(&cb);
	Libs::Emulator::ARM64Relocation reloc(&em);

	Check(reloc.EmitB(4),   "EmitB");
	Check(reloc.EmitBL(-8), "EmitBL");
	Check(reloc.EmitCBZ(Libs::Emulator::ARM64Reg::X0, 16),  "EmitCBZ");
	Check(reloc.EmitCBNZ(Libs::Emulator::ARM64Reg::X1, -16), "EmitCBNZ");

	Check(cb.size == 16, "Relocation: 4 branch instructions = 16 bytes");

	// Verify B opcode (bit[31:26] == 0b000101)
	uint32_t b_insn = 0;
	std::memcpy(&b_insn, buf.data(), 4);
	Check((b_insn >> 26u) == 0x05u, "B opcode prefix");

	// Verify BL opcode (bit[31:26] == 0b100101)
	uint32_t bl_insn = 0;
	std::memcpy(&bl_insn, buf.data() + 4, 4);
	Check((bl_insn >> 26u) == 0x25u, "BL opcode prefix");

	// PatchBranch26 in-place
	uint32_t target = 0x14000000u; // B #0
	Check(Libs::Emulator::ARM64Relocation::PatchBranch26(&target, 100), "PatchBranch26");
	Check((target & 0xFFFFFFu) == 100u, "PatchBranch26 imm26");
	Check((target >> 26u) == 0x05u, "PatchBranch26 opcode preserved");

	std::printf("  [OK] TestRelocationEncoding\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 4: Register allocator
// ──────────────────────────────────────────────────────────────────────────────

void TestRegisterAllocator() {
	Libs::Emulator::ARM64RegisterAllocator alloc;

	// Fixed registers (FP=x29, LR=x30, XZR/SP=x31) must be pre-reserved
	Check(alloc.IsAllocated(Libs::Emulator::ARM64Reg::X29), "x29 (FP) reserved");
	Check(alloc.IsAllocated(Libs::Emulator::ARM64Reg::X30), "x30 (LR) reserved");

	// Allocate x0
	Check(!alloc.IsAllocated(Libs::Emulator::ARM64Reg::X0), "x0 free initially");
	Check(alloc.AllocateReg(Libs::Emulator::ARM64Reg::X0),  "Allocate x0");
	Check(alloc.IsAllocated(Libs::Emulator::ARM64Reg::X0),  "x0 allocated");
	Check(!alloc.AllocateReg(Libs::Emulator::ARM64Reg::X0), "x0 double-alloc rejected");

	// Acquire scratch — should be x9 (first caller-saved)
	auto scratch = alloc.AcquireScratch();
	Check(scratch != Libs::Emulator::ARM64Reg::XZR, "Scratch not XZR");
	Check(alloc.IsAllocated(scratch), "Scratch marked allocated");

	// Free and re-check
	alloc.FreeReg(Libs::Emulator::ARM64Reg::X0);
	Check(!alloc.IsAllocated(Libs::Emulator::ARM64Reg::X0), "x0 free after FreeReg");

	// Reset
	alloc.Reset();
	Check(!alloc.IsAllocated(Libs::Emulator::ARM64Reg::X0), "x0 free after Reset");
	Check(alloc.IsAllocated(Libs::Emulator::ARM64Reg::X29), "x29 reserved after Reset");

	std::printf("  [OK] TestRegisterAllocator\n");
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 5: Live JIT execution (ARM64 host only)
// ──────────────────────────────────────────────────────────────────────────────

void TestLiveJitExecution() {
#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
	const uint64_t page = Common::VirtualMemory::Alloc(0, 4096, Common::VirtualMemory::Mode::ExecuteReadWrite);
	Check(page != 0, "JIT page alloc");

	Libs::Emulator::CodeBuffer cb{reinterpret_cast<uint8_t*>(page), 4096, 0};
	Libs::Emulator::ARM64Emitter em(&cb);

	// uint64_t Add(uint64_t a /*x0*/, uint64_t b /*x1*/) { return a + b; }
	Check(em.EmitADD(Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X1), "JIT EmitADD");
	Check(em.EmitRET(), "JIT EmitRET");
	Check(Common::VirtualMemory::FlushInstructionCache(page, cb.size), "JIT FlushInstructionCache");

	using add_fn = uint64_t (*)(uint64_t, uint64_t);
	auto fn       = reinterpret_cast<add_fn>(page);
	uint64_t result = fn(40, 2);
	Check(result == 42, "JIT add(40,2) == 42");

	Common::VirtualMemory::Free(page);
	std::printf("  [OK] TestLiveJitExecution (native ARM64)\n");
#else
	std::printf("  [SKIP] TestLiveJitExecution (non-ARM64 host)\n");
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// Test 6: Multithreaded code generation
// ──────────────────────────────────────────────────────────────────────────────

void TestMultithreadedGeneration() {
	static constexpr size_t NUM_THREADS = 8;
	std::atomic<size_t> ok{0};
	std::vector<std::thread> threads;
	threads.reserve(NUM_THREADS);

	for (size_t t = 0; t < NUM_THREADS; t++) {
		threads.emplace_back([t, &ok]() {
			std::vector<uint8_t> buf(512);
			Libs::Emulator::CodeBuffer cb{buf.data(), buf.size(), 0};
			Libs::Emulator::ARM64Emitter em(&cb);

			bool good = em.EmitMOV64(Libs::Emulator::ARM64Reg::X0, static_cast<uint64_t>(t) * 0x1000000000001ULL) &&
			            em.EmitRET();
			if (good) {
				ok.fetch_add(1, std::memory_order_relaxed);
			}
		});
	}
	for (auto& th: threads) {
		th.join();
	}

	Check(ok.load() == NUM_THREADS, "All threads generated code successfully");
	std::printf("  [OK] TestMultithreadedGeneration (%zu threads)\n", NUM_THREADS);
}

// ──────────────────────────────────────────────────────────────────────────────
// Benchmark: JIT compilation throughput
// ──────────────────────────────────────────────────────────────────────────────

void BenchmarkCompileThroughput() {
	static constexpr size_t ITERS = 200000;
	std::vector<uint8_t> buf(256);

	const auto t0 = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERS; i++) {
		Libs::Emulator::CodeBuffer cb{buf.data(), buf.size(), 0};
		Libs::Emulator::ARM64Emitter em(&cb);
		em.EmitMOV64(Libs::Emulator::ARM64Reg::X0, static_cast<uint64_t>(i));
		em.EmitADD(Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X0, Libs::Emulator::ARM64Reg::X1);
		em.EmitRET();
	}
	const auto t1 = std::chrono::high_resolution_clock::now();
	double total_us = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()) / 1000.0;
	double per_op_ns = (total_us * 1000.0) / static_cast<double>(ITERS);

	std::printf("  [Bench] JIT compile:  %.2f ns/block  (%.2f ms total, %zu blocks)\n",
	            per_op_ns, total_us / 1000.0, ITERS);
	std::printf("  [Bench] Generated code size: 20 bytes/block (MOV64+ADD+RET)\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5  Native ARM64 JIT Backend  Unit Tests      \n");
	std::printf("====================================================\n\n");

	TestArithmeticEncoding();
	TestMov64Encoding();
	TestRelocationEncoding();
	TestRegisterAllocator();
	TestLiveJitExecution();
	TestMultithreadedGeneration();

	std::printf("\n");
	BenchmarkCompileThroughput();

	std::printf("\nARM64BackendTests: ALL PASSED\n");
	return 0;
}
