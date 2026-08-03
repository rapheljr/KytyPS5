#ifndef EMULATOR_INCLUDE_EMULATOR_ARM64EMITTER_H_
#define EMULATOR_INCLUDE_EMULATOR_ARM64EMITTER_H_

#include "emulator/arm64BackendInterface.h"

#include <cstdint>

namespace Libs::Emulator {

enum class ARM64Reg : uint8_t {
	X0 = 0,   X1,  X2,  X3,  X4,  X5,  X6,  X7,
	X8,       X9,  X10, X11, X12, X13, X14, X15,
	X16,      X17, X18, X19, X20, X21, X22, X23,
	X24,      X25, X26, X27, X28, X29, X30, SP = 31, XZR = 31
};

class ARM64Emitter {
public:
	explicit ARM64Emitter(CodeBuffer* buffer) noexcept : m_buffer(buffer) {}

	[[nodiscard]] CodeBuffer* GetBuffer() const noexcept { return m_buffer; }

	bool EmitNOP();
	bool EmitRET(ARM64Reg rn = ARM64Reg::X30);
	bool EmitMOVZ(ARM64Reg rd, uint16_t imm16, uint8_t shift = 0);
	bool EmitMOVK(ARM64Reg rd, uint16_t imm16, uint8_t shift = 0);
	bool EmitMOV64(ARM64Reg rd, uint64_t imm64);
	bool EmitADD(ARM64Reg rd, ARM64Reg rn, ARM64Reg rm);
	bool EmitSUB(ARM64Reg rd, ARM64Reg rn, ARM64Reg rm);
	bool EmitBLR(ARM64Reg rn);
	bool EmitSVC(uint16_t imm16);

	// Pair memory ops (64-bit GPR)
	bool EmitSTP_PreIndex(ARM64Reg rt1, ARM64Reg rt2, ARM64Reg rn, int32_t simm7);
	bool EmitLDP_PostIndex(ARM64Reg rt1, ARM64Reg rt2, ARM64Reg rn, int32_t simm7);

private:
	CodeBuffer* m_buffer = nullptr;
};

} // namespace Libs::Emulator

#endif // EMULATOR_INCLUDE_EMULATOR_ARM64EMITTER_H_
