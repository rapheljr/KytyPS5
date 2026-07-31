#include "common/config.h"

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
#elif defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
	Check(KYTY_ARCH == KYTY_ARCH_ARM64, "KYTY_ARCH should be KYTY_ARCH_ARM64 on ARM64 build");
#else
	Check(false, "Unknown host architecture target");
#endif
}

void TestPointerSizeAndEndian() {
	Check(sizeof(void*) == 8, "Host architecture must be 64-bit");
	Check(KYTY_ENDIAN == KYTY_ENDIAN_LITTLE || KYTY_ENDIAN == KYTY_ENDIAN_BIG,
	      "KYTY_ENDIAN must be valid");
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
	TestPlatformMacros();

	std::printf("ArchitectureConfigTests: PASSED\n");
	return 0;
}
