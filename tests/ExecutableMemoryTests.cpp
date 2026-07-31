#include "common/platform/sysVirtual.h"
#include "common/config.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ExecutableMemoryTests: failed: %s\n", text);
		std::abort();
	}
}

void TestExecutableAllocationAndFlush() {
	constexpr size_t alloc_size = 0x4000;
	uint64_t vaddr = Common::SysVirtualAlloc(
	    0, alloc_size, Common::VirtualMemory::Mode::ExecuteReadWrite);
	Check(vaddr != 0, "SysVirtualAlloc with ExecuteReadWrite must succeed");

	auto* code_ptr = reinterpret_cast<uint8_t*>(vaddr);

#if defined(__x86_64__) || defined(_M_X64)
	// Write x86 RET (0xc3)
	code_ptr[0] = 0xc3;
#elif defined(__aarch64__) || defined(_M_ARM64)
	// Write ARM64 RET (0xd65f03c0)
	auto* code_u32 = reinterpret_cast<uint32_t*>(vaddr);
	code_u32[0] = 0xd65f03c0;
#else
	code_ptr[0] = 0;
#endif

	bool flushed = Common::SysVirtualFlushInstructionCache(vaddr, alloc_size);
	Check(flushed, "SysVirtualFlushInstructionCache must succeed");

	bool freed = Common::SysVirtualFree(vaddr);
	Check(freed, "SysVirtualFree must succeed for executable memory");
}

} // namespace

int main() {
	TestExecutableAllocationAndFlush();

	std::printf("ExecutableMemoryTests: PASSED\n");
	return 0;
}
