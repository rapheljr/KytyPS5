#include "common/common.h"
#include "common/config.h"
#include "common/hostArchitecture.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ArchitectureConfigTests: failed: %s\n", text);
		std::abort();
	}
}

void TestArchitectureMacros() {
#if defined(__x86_64__) || defined(_M_X64)
	Check(KYTY_ARCH == KYTY_ARCH_X86_64, "KYTY_ARCH should be KYTY_ARCH_X86_64 on x86_64 build");
	Check(KYTY_HOST_ARCH_X86_64 == 1, "KYTY_HOST_ARCH_X86_64 macro should be 1");
	Check(Common::HostArchitecture::IsX86_64(), "HostArchitecture::IsX86_64() should return true");
	Check(!Common::HostArchitecture::IsARM64(), "HostArchitecture::IsARM64() should return false");
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
	Check(KYTY_ARCH == KYTY_ARCH_ARM64, "KYTY_ARCH should be KYTY_ARCH_ARM64 on ARM64 build");
	Check(KYTY_HOST_ARCH_ARM64 == 1, "KYTY_HOST_ARCH_ARM64 macro should be 1");
	Check(Common::HostArchitecture::IsARM64(), "HostArchitecture::IsARM64() should return true");
	Check(!Common::HostArchitecture::IsX86_64(), "HostArchitecture::IsX86_64() should return false");
#else
	Check(false, "Unknown host architecture target");
#endif
	Check(Common::HostArchitecture::GetArchitectureName() != nullptr, "GetArchitectureName() should not be null");
}

void TestPointerSizeAndEndian() {
	Check(sizeof(void*) == 8, "Host architecture must be 64-bit");
	Check(KYTY_ENDIAN == KYTY_ENDIAN_LITTLE || KYTY_ENDIAN == KYTY_ENDIAN_BIG,
	      "KYTY_ENDIAN must be valid");
}

void TestHostRegisterContext() {
	Check(sizeof(Common::HostArchitecture::HostRegisterContext) >= 288,
	      "HostRegisterContext size must accommodate 32 GPRs + PC/SP/FP/Flags");
	Common::HostArchitecture::HostRegisterContext ctx {};
	ctx.pc = 0x1000;
	ctx.sp = 0x2000;
	ctx.gpr[0] = 0x42;
	Check(ctx.pc == 0x1000 && ctx.sp == 0x2000 && ctx.gpr[0] == 0x42,
	      "HostRegisterContext read/write failed");
}

void TestPlatformMacros() {
#if KYTY_PLATFORM == KYTY_PLATFORM_MACOS
	Check(KYTY_PLATFORM == 3, "KYTY_PLATFORM_MACOS value mismatch");
#elif KYTY_PLATFORM == KYTY_PLATFORM_LINUX
	Check(KYTY_PLATFORM == 5, "KYTY_PLATFORM_LINUX value mismatch");
#elif KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	Check(KYTY_PLATFORM == 1, "KYTY_PLATFORM_WINDOWS value mismatch");
#else
	Check(false, "KYTY_PLATFORM must be valid");
#endif
}

} // namespace

int main() {
	TestArchitectureMacros();
	TestPointerSizeAndEndian();
	TestHostRegisterContext();
	TestPlatformMacros();

	std::printf("ArchitectureConfigTests: PASSED\n");
	return 0;
}
