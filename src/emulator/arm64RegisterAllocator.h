#ifndef EMULATOR_INCLUDE_EMULATOR_ARM64REGISTERALLOCATOR_H_
#define EMULATOR_INCLUDE_EMULATOR_ARM64REGISTERALLOCATOR_H_

#include "emulator/arm64Emitter.h"

#include <array>
#include <cstdint>

namespace Libs::Emulator {

class ARM64RegisterAllocator {
public:
	ARM64RegisterAllocator() noexcept;

	[[nodiscard]] bool IsAllocated(ARM64Reg reg) const noexcept;
	[[nodiscard]] bool AllocateReg(ARM64Reg reg) noexcept;
	void               FreeReg(ARM64Reg reg) noexcept;
	void               Reset() noexcept;

	[[nodiscard]] ARM64Reg AcquireScratch() noexcept;

private:
	std::array<bool, 32> m_gpr_allocated {};
};

} // namespace Libs::Emulator

#endif // EMULATOR_INCLUDE_EMULATOR_ARM64REGISTERALLOCATOR_H_
