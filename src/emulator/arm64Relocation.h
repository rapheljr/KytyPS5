#ifndef EMULATOR_INCLUDE_EMULATOR_ARM64RELOCATION_H_
#define EMULATOR_INCLUDE_EMULATOR_ARM64RELOCATION_H_

#include "emulator/arm64Emitter.h"

#include <cstdint>

namespace Libs::Emulator {

class ARM64Relocation {
public:
	explicit ARM64Relocation(ARM64Emitter* emitter) noexcept : m_emitter(emitter) {}

	bool EmitB(int32_t offset26_instructions);
	bool EmitBL(int32_t offset26_instructions);
	bool EmitCBZ(ARM64Reg rt, int32_t offset19_instructions);
	bool EmitCBNZ(ARM64Reg rt, int32_t offset19_instructions);

	static bool PatchBranch26(uint32_t* instruction_addr, int32_t offset26_instructions);

private:
	ARM64Emitter* m_emitter = nullptr;
};

} // namespace Libs::Emulator

#endif // EMULATOR_INCLUDE_EMULATOR_ARM64RELOCATION_H_
