// arm64RegisterAllocator.h
//
// Dynamic GPR & SIMD Register Allocator with Spill & Reload Management.

#ifndef LOADER_RECOMPILER_ARM64_REGISTER_ALLOCATOR_H
#define LOADER_RECOMPILER_ARM64_REGISTER_ALLOCATOR_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/arm64FpSimdEmitter.h"

#include <array>
#include <cstdint>

namespace Loader::Recompiler {

class Arm64RegisterAllocator {
public:
	Arm64RegisterAllocator() { Reset(); }
	~Arm64RegisterAllocator() = default;

	KYTY_CLASS_NO_COPY(Arm64RegisterAllocator);

	void Reset() noexcept;

	// GPR Allocation (X0 - X15)
	[[nodiscard]] Arm64Reg AllocateGpr();
	void FreeGpr(Arm64Reg reg) noexcept;
	[[nodiscard]] bool IsGprAllocated(Arm64Reg reg) const noexcept;

	// SIMD/Vector Allocation (V0 - V15)
	[[nodiscard]] Arm64FpReg AllocateVectorReg();
	void FreeVectorReg(Arm64FpReg reg) noexcept;
	[[nodiscard]] bool IsVectorRegAllocated(Arm64FpReg reg) const noexcept;

	[[nodiscard]] uint32_t GetAllocatedGprCount() const noexcept;
	[[nodiscard]] uint32_t GetAllocatedVectorCount() const noexcept;

private:
	std::array<bool, 16> m_gpr_allocated{};
	std::array<bool, 16> m_vec_allocated{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_REGISTER_ALLOCATOR_H
