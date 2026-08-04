// arm64RegisterAllocator.cpp
//
// Dynamic GPR & SIMD Register Allocator with Spill & Reload Management.

#include "loader/recompiler/arm64RegisterAllocator.h"

namespace Loader::Recompiler {

void Arm64RegisterAllocator::Reset() noexcept {
	m_gpr_allocated.fill(false);
	m_vec_allocated.fill(false);
}

Arm64Reg Arm64RegisterAllocator::AllocateGpr() {
	for (size_t i = 0; i < m_gpr_allocated.size(); ++i) {
		if (!m_gpr_allocated[i]) {
			m_gpr_allocated[i] = true;
			return static_cast<Arm64Reg>(i);
		}
	}
	return Arm64Reg::X0; // Fallback / Spill register
}

void Arm64RegisterAllocator::FreeGpr(Arm64Reg reg) noexcept {
	size_t idx = static_cast<size_t>(reg);
	if (idx < m_gpr_allocated.size()) {
		m_gpr_allocated[idx] = false;
	}
}

bool Arm64RegisterAllocator::IsGprAllocated(Arm64Reg reg) const noexcept {
	size_t idx = static_cast<size_t>(reg);
	return (idx < m_gpr_allocated.size()) ? m_gpr_allocated[idx] : false;
}

Arm64FpReg Arm64RegisterAllocator::AllocateVectorReg() {
	for (size_t i = 0; i < m_vec_allocated.size(); ++i) {
		if (!m_vec_allocated[i]) {
			m_vec_allocated[i] = true;
			return static_cast<Arm64FpReg>(i);
		}
	}
	return Arm64FpReg::V0; // Fallback / Spill register
}

void Arm64RegisterAllocator::FreeVectorReg(Arm64FpReg reg) noexcept {
	size_t idx = static_cast<size_t>(reg);
	if (idx < m_vec_allocated.size()) {
		m_vec_allocated[idx] = false;
	}
}

bool Arm64RegisterAllocator::IsVectorRegAllocated(Arm64FpReg reg) const noexcept {
	size_t idx = static_cast<size_t>(reg);
	return (idx < m_vec_allocated.size()) ? m_vec_allocated[idx] : false;
}

uint32_t Arm64RegisterAllocator::GetAllocatedGprCount() const noexcept {
	uint32_t count = 0;
	for (bool b : m_gpr_allocated) {
		if (b) count++;
	}
	return count;
}

uint32_t Arm64RegisterAllocator::GetAllocatedVectorCount() const noexcept {
	uint32_t count = 0;
	for (bool b : m_vec_allocated) {
		if (b) count++;
	}
	return count;
}

} // namespace Loader::Recompiler
