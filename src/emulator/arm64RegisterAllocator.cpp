#include "emulator/arm64RegisterAllocator.h"

namespace Libs::Emulator {

ARM64RegisterAllocator::ARM64RegisterAllocator() noexcept {
	Reset();
}

bool ARM64RegisterAllocator::IsAllocated(ARM64Reg reg) const noexcept {
	const auto idx = static_cast<size_t>(reg);
	if (idx >= 32) {
		return false;
	}
	return m_gpr_allocated[idx];
}

bool ARM64RegisterAllocator::AllocateReg(ARM64Reg reg) noexcept {
	const auto idx = static_cast<size_t>(reg);
	if (idx >= 32 || m_gpr_allocated[idx]) {
		return false;
	}
	m_gpr_allocated[idx] = true;
	return true;
}

void ARM64RegisterAllocator::FreeReg(ARM64Reg reg) noexcept {
	const auto idx = static_cast<size_t>(reg);
	if (idx < 32) {
		m_gpr_allocated[idx] = false;
	}
}

void ARM64RegisterAllocator::Reset() noexcept {
	m_gpr_allocated.fill(false);
	// Reserve fixed registers FP (x29), LR (x30), SP/XZR (x31)
	m_gpr_allocated[29] = true;
	m_gpr_allocated[30] = true;
	m_gpr_allocated[31] = true;
}

ARM64Reg ARM64RegisterAllocator::AcquireScratch() noexcept {
	// Allocate temporary scratch registers x9-x15 first (caller-saved)
	for (uint8_t r = 9; r <= 15; r++) {
		if (!m_gpr_allocated[r]) {
			m_gpr_allocated[r] = true;
			return static_cast<ARM64Reg>(r);
		}
	}
	// Fall back to x0-x8
	for (uint8_t r = 0; r <= 8; r++) {
		if (!m_gpr_allocated[r]) {
			m_gpr_allocated[r] = true;
			return static_cast<ARM64Reg>(r);
		}
	}
	return ARM64Reg::XZR;
}

} // namespace Libs::Emulator
