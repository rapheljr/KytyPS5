#ifndef EMULATOR_INCLUDE_EMULATOR_ARM64BACKENDINTERFACE_H_
#define EMULATOR_INCLUDE_EMULATOR_ARM64BACKENDINTERFACE_H_

#include <cstddef>
#include <cstdint>

namespace Libs::Emulator {

// ──────────────────────────────────────────────────────────────────────────────
// Architecture tag returned by IARM64JitBackend::GetArchitecture()
// ──────────────────────────────────────────────────────────────────────────────

enum class JitHostArch : uint8_t {
	Unknown = 0,
	X86_64  = 1,
	ARM64   = 2,
};

// ──────────────────────────────────────────────────────────────────────────────
// Code buffer: a flat byte span + write cursor
// Lifetime: owned externally; this struct is non-owning.
// ──────────────────────────────────────────────────────────────────────────────

struct CodeBuffer {
	uint8_t* ptr      = nullptr;
	size_t   capacity = 0;
	size_t   size     = 0;

	void Reset() noexcept { size = 0; }

	bool EmitByte(uint8_t b) noexcept {
		if (size >= capacity) { return false; }
		ptr[size++] = b;
		return true;
	}

	bool Emit32(uint32_t val) noexcept {
		if (size + sizeof(uint32_t) > capacity) { return false; }
		ptr[size]     = static_cast<uint8_t>(val);
		ptr[size + 1] = static_cast<uint8_t>(val >> 8u);
		ptr[size + 2] = static_cast<uint8_t>(val >> 16u);
		ptr[size + 3] = static_cast<uint8_t>(val >> 24u);
		size += sizeof(uint32_t);
		return true;
	}

	bool Emit64(uint64_t val) noexcept {
		if (size + sizeof(uint64_t) > capacity) { return false; }
		for (int i = 0; i < 8; ++i) {
			ptr[size + static_cast<size_t>(i)] = static_cast<uint8_t>(val >> (8u * static_cast<unsigned>(i)));
		}
		size += sizeof(uint64_t);
		return true;
	}
};

// ──────────────────────────────────────────────────────────────────────────────
// Abstract JIT backend interface
// ──────────────────────────────────────────────────────────────────────────────

class IARM64JitBackend {
public:
	virtual ~IARM64JitBackend() = default;

	[[nodiscard]] virtual JitHostArch GetArchitecture() const noexcept = 0;
	[[nodiscard]] virtual bool        GenerateCode(CodeBuffer* buffer)  = 0;
	virtual void                      InvalidateCache(uint64_t address, size_t size) = 0;
};

} // namespace Libs::Emulator

#endif // EMULATOR_INCLUDE_EMULATOR_ARM64BACKENDINTERFACE_H_
