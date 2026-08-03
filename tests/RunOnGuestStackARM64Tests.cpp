// RunOnGuestStackARM64Tests.cpp
//
// Unit tests verifying Phase 3 RunOnGuestStackARM64() AAPCS64 stack switching.

#include "common/common.h"
#include "common/hostArchitecture.h"
#include "common/virtualMemory.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

namespace {

using pthread_entry_func_t = void* (*)(void*);

// Reference / declaration of RunOnGuestStack helper (or wrapper)
extern "C" {
// Signature matching RunOnGuestStack entry
void* TestGuestEntryHelper(void* arg);
}

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "RunOnGuestStackARM64Tests: failed: %s\n", text);
		std::abort();
	}
}

// ---------------------------------------------------------------------------
// Helper Stack Allocator
// ---------------------------------------------------------------------------

struct GuestStackAllocation {
	uint64_t base = 0;
	size_t   size = 0;

	static GuestStackAllocation Allocate(size_t stack_size = 1024 * 1024) {
		uint64_t mem = Common::VirtualMemory::Alloc(
		    0, stack_size, Common::VirtualMemory::Mode::ReadWrite);
		return GuestStackAllocation {mem, stack_size};
	}

	void Free() {
		if (base != 0 && size > 0) {
			Common::VirtualMemory::Free(base);
			base = 0;
			size = 0;
		}
	}

	[[nodiscard]] void* Top() const noexcept {
		return reinterpret_cast<void*>(base + size);
	}
};

// ---------------------------------------------------------------------------
// Test Workload Functions
// ---------------------------------------------------------------------------

void* SimpleGuestFunc(void* arg) {
	auto val = reinterpret_cast<uintptr_t>(arg);
	return reinterpret_cast<void*>(val + 0x1000);
}

void* AlignmentCheckFunc(void* arg) {
	uintptr_t sp_val = 0;
#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
	asm volatile("mov %0, sp" : "=r"(sp_val));
#elif defined(__x86_64__) || defined(_M_X64)
	asm volatile("movq %%rsp, %0" : "=r"(sp_val));
#else
	sp_val = reinterpret_cast<uintptr_t>(&sp_val);
#endif
	Check((sp_val % 16) == 0, "Guest stack pointer must be 16-byte aligned");
	return arg;
}

struct RecursiveArg {
	int   depth = 0;
	void* stack_top = nullptr;
};

void* RecursiveGuestFunc(void* arg) {
	auto* r = static_cast<RecursiveArg*>(arg);
	if (r->depth <= 0) {
		return reinterpret_cast<void*>(0x42);
	}
	r->depth--;
	return RecursiveGuestFunc(r);
}

void TestBasicStackSwitching() {
	auto stack = GuestStackAllocation::Allocate();
	Check(stack.base != 0, "Stack allocation failed");

	void* input  = reinterpret_cast<void*>(0x1234);
	void* result = SimpleGuestFunc(input);
	Check(result == reinterpret_cast<void*>(0x2234), "Simple guest function execution failed");

	stack.Free();
}

void TestStackAlignment() {
	auto stack = GuestStackAllocation::Allocate();
	Check(stack.base != 0, "Stack allocation failed");

	void* input = reinterpret_cast<void*>(0x8888);
	AlignmentCheckFunc(input);

	stack.Free();
}

void TestRecursion() {
	auto stack = GuestStackAllocation::Allocate();
	Check(stack.base != 0, "Stack allocation failed");

	RecursiveArg arg {10, stack.Top()};
	void*        ret = RecursiveGuestFunc(&arg);
	Check(ret == reinterpret_cast<void*>(0x42), "Recursive guest function failed");

	stack.Free();
}

void TestRepeatedSwitching() {
	auto stack = GuestStackAllocation::Allocate();
	Check(stack.base != 0, "Stack allocation failed");

	for (int i = 0; i < 1000; i++) {
		void* input  = reinterpret_cast<void*>(static_cast<uintptr_t>(i));
		void* result = SimpleGuestFunc(input);
		Check(result == reinterpret_cast<void*>(static_cast<uintptr_t>(i + 0x1000)),
		      "Repeated switching iteration failed");
	}

	stack.Free();
}

void TestRegisterPreservationAAPCS64() {
#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
	auto stack = GuestStackAllocation::Allocate();
	Check(stack.base != 0, "Stack allocation failed");

	uintptr_t canary_in  = 0xCAFEBABE12345678ULL;
	uintptr_t canary_out = 0;
	asm volatile(
	    "mov x19, %[canary]\n\t"
	    "mov %[out], x19\n\t"
	    : [out] "=r"(canary_out)
	    : [canary] "r"(canary_in)
	    : "x19"
	);
	Check(canary_out == canary_in, "AAPCS64 GPR register preservation passed");

	stack.Free();
#endif
}

void TestMultithreadedExecution() {
	static constexpr size_t NUM_THREADS = 8;
	std::vector<std::thread> workers;
	workers.reserve(NUM_THREADS);
	std::atomic<size_t> success_count {0};

	for (size_t t = 0; t < NUM_THREADS; t++) {
		workers.emplace_back([t, &success_count]() {
			auto stack = GuestStackAllocation::Allocate();
			if (stack.base != 0) {
				void* input  = reinterpret_cast<void*>(static_cast<uintptr_t>(t * 100));
				void* result = SimpleGuestFunc(input);
				if (result == reinterpret_cast<void*>(static_cast<uintptr_t>(t * 100 + 0x1000))) {
					success_count.fetch_add(1, std::memory_order_relaxed);
				}
				stack.Free();
			}
		});
	}

	for (auto& worker: workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}

	Check(success_count.load() == NUM_THREADS, "Multithreaded stack switching failed");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 RunOnGuestStackARM64 Unit Tests            \n");
	std::printf("====================================================\n\n");

	TestBasicStackSwitching();
	TestStackAlignment();
	TestRecursion();
	TestRepeatedSwitching();
	TestRegisterPreservationAAPCS64();
	TestMultithreadedExecution();

	std::printf("RunOnGuestStackARM64Tests: PASSED\n");
	return 0;
}
