#ifndef EMULATOR_INCLUDE_EMULATOR_LOADER_ARM64JIT_H_
#define EMULATOR_INCLUDE_EMULATOR_LOADER_ARM64JIT_H_

// ARM64 JIT trampoline stubs for native ARM64 host execution.
// These replace the x86_64-specific equivalents in jit.h on ARM64 targets.
// Only compiled / linked when targeting AArch64 host.

#include <cstdint>
#include <cstring>

namespace Loader::JitARM64 {

#pragma pack(1)

// Encodes an 8-instruction sequence that loads <index> into x16 and
// jumps indirectly to a handler loaded into x17.
// Total size: 24 bytes (6 x 32-bit ARM64 instructions).
struct JmpWithIndexARM64 {
	void SetIndex(uint32_t index) {
		const uint32_t imm16 = index & 0xFFFFu;
		// movz x16, #imm16
		Write32(&code[0], 0xD2800000u | (imm16 << 5u) | 16u);
	}

	void SetFunc(void* handler) {
		const auto h = reinterpret_cast<uint64_t>(handler);
		// movz x17, h[0:15]
		Write32(&code[4], 0xD2800000u | ((h & 0xFFFFu) << 5u) | 17u);
		// movk x17, h[16:31], lsl #16
		Write32(&code[8], 0xF2A00000u | (((h >> 16u) & 0xFFFFu) << 5u) | 17u);
		// movk x17, h[32:47], lsl #32
		Write32(&code[12], 0xF2C00000u | (((h >> 32u) & 0xFFFFu) << 5u) | 17u);
		// movk x17, h[48:63], lsl #48
		Write32(&code[16], 0xF2E00000u | (((h >> 48u) & 0xFFFFu) << 5u) | 17u);
		// br x17
		Write32(&code[20], 0xD61F0220u);
	}

	[[nodiscard]] static uint64_t GetSize() noexcept { return sizeof(code); }

	uint8_t code[24] = {};

private:
	static void Write32(void* dst, uint32_t val) {
		std::memcpy(dst, &val, sizeof(val));
	}
};

struct SafeCallARM64 {
	using handler_t = void (*)();

	void SetFunc(handler_t func) {
		const auto h = reinterpret_cast<uint64_t>(reinterpret_cast<void*>(func));
		// movz x16, h[0:15]
		Write32(&code[0], 0xD2800000u | ((h & 0xFFFFu) << 5u) | 16u);
		// movk x16, h[16:31], lsl #16
		Write32(&code[4], 0xF2A00000u | (((h >> 16u) & 0xFFFFu) << 5u) | 16u);
		// movk x16, h[32:47], lsl #32
		Write32(&code[8], 0xF2C00000u | (((h >> 32u) & 0xFFFFu) << 5u) | 16u);
		// movk x16, h[48:63], lsl #48
		Write32(&code[12], 0xF2E00000u | (((h >> 48u) & 0xFFFFu) << 5u) | 16u);
		// blr x16
		Write32(&code[16], 0xD63F0200u);
		// ret
		Write32(&code[20], 0xD65F03C0u);
	}

	[[nodiscard]] static uint64_t GetSize() noexcept { return sizeof(code); }

	uint8_t code[64] = {};

private:
	static void Write32(void* dst, uint32_t val) {
		std::memcpy(dst, &val, sizeof(val));
	}
};

#pragma pack()

} // namespace Loader::JitARM64

#endif // EMULATOR_INCLUDE_EMULATOR_LOADER_ARM64JIT_H_
