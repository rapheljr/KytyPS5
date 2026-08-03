#ifndef KYTY_COMMON_HOST_ARCHITECTURE_H_
#define KYTY_COMMON_HOST_ARCHITECTURE_H_

#include <cstddef>
#include <cstdint>

// ---------------------------------------------------------------------------
// Host Architecture Detection Macros
// ---------------------------------------------------------------------------

#if defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
#	define KYTY_HOST_ARCH_ARM64 1
#	define KYTY_HOST_ARCH_NAME "arm64"
#elif defined(__x86_64__) || defined(_M_X64)
#	define KYTY_HOST_ARCH_X86_64 1
#	define KYTY_HOST_ARCH_NAME "x86_64"
#else
#	error "Unsupported host architecture"
#endif

namespace Common::HostArchitecture {

// ---------------------------------------------------------------------------
// Unified Host Register Context Abstraction
// ---------------------------------------------------------------------------

struct HostRegisterContext {
	uint64_t pc    = 0; // Program Counter (pc on ARM64, rip on x86_64)
	uint64_t sp    = 0; // Stack Pointer (sp / x31 on ARM64, rsp on x86_64)
	uint64_t fp    = 0; // Frame Pointer (fp / x29 on ARM64, rbp on x86_64)
	uint64_t flags = 0; // Processor Flags / Syndrome (pstate/esr on ARM64, rflags on x86_64)

	// 32 General-Purpose Register slots:
	//   ARM64:  gpr[0..30] map to x0..x30 (gpr[31] = sp)
	//   x86_64: gpr[0..15] map to rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp, r8..r15
	uint64_t gpr[32] = {};
};

// ---------------------------------------------------------------------------
// Helper Query Functions
// ---------------------------------------------------------------------------

[[nodiscard]] constexpr const char* GetArchitectureName() noexcept {
	return KYTY_HOST_ARCH_NAME;
}

[[nodiscard]] constexpr bool IsARM64() noexcept {
#if defined(KYTY_HOST_ARCH_ARM64)
	return true;
#else
	return false;
#endif
}

[[nodiscard]] constexpr bool IsX86_64() noexcept {
#if defined(KYTY_HOST_ARCH_X86_64)
	return true;
#else
	return false;
#endif
}

} // namespace Common::HostArchitecture

#endif /* KYTY_COMMON_HOST_ARCHITECTURE_H_ */
