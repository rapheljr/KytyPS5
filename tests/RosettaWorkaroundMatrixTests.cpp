#include "common/config.h"
#include "common/platform/sysVirtual.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
#include <mach/mach.h>
#include <pthread.h>
#endif

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "RosettaWorkaroundMatrixTests: failed: %s\n", text);
		std::abort();
	}
}

void TestAudit01ArchitectureConfig() {
#if KYTY_ARCH == KYTY_ARCH_X86_64
	Check(sizeof(void*) == 8, "x86_64 must be 64-bit");
#elif KYTY_ARCH == KYTY_ARCH_ARM64
	Check(sizeof(void*) == 8, "ARM64 must be 64-bit");
#else
	Check(false, "KYTY_ARCH must be defined as X86_64 or ARM64");
#endif
}

void TestAudit06TlsAndThreadIdentity() {
#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
	mach_port_t self_port1 = pthread_mach_thread_np(pthread_self());
	mach_port_t self_port2 = pthread_mach_thread_np(pthread_self());
	Check(self_port1 != 0, "pthread_mach_thread_np port must be non-zero");
	Check(self_port1 == self_port2, "pthread_mach_thread_np port must be consistent");
#endif
}

void TestAudit08And09HostPageSizeAndExecutableMemory() {
	const auto system_page_size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
	Check(system_page_size >= 4096, "Host page size must be at least 4 KB");

	const uint64_t vaddr = Common::SysVirtualAlloc(
	    0, system_page_size, Common::VirtualMemory::Mode::ExecuteReadWrite);
	Check(vaddr != 0, "SysVirtualAlloc ExecuteReadWrite must succeed");

	const bool protected_ok = Common::SysVirtualProtect(
	    vaddr, system_page_size, Common::VirtualMemory::Mode::ExecuteRead, nullptr);
	Check(protected_ok, "SysVirtualProtect to ExecuteRead must succeed");

	const bool flushed = Common::SysVirtualFlushInstructionCache(vaddr, system_page_size);
	Check(flushed, "SysVirtualFlushInstructionCache must succeed");

	const bool freed = Common::SysVirtualFree(vaddr);
	Check(freed, "SysVirtualFree must succeed");
}

} // namespace

int main() {
	TestAudit01ArchitectureConfig();
	TestAudit06TlsAndThreadIdentity();
	TestAudit08And09HostPageSizeAndExecutableMemory();

	std::printf("RosettaWorkaroundMatrixTests: PASSED\n");
	return 0;
}
