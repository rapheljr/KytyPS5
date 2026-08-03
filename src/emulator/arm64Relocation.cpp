#include "emulator/arm64Relocation.h"

namespace Libs::Emulator {

bool ARM64Relocation::EmitB(int32_t offset26_instructions) {
	if (m_emitter == nullptr || m_emitter->GetBuffer() == nullptr) {
		return false;
	}
	const uint32_t imm26 = static_cast<uint32_t>(offset26_instructions) & 0x03FFFFFFu;
	const uint32_t op    = 0x14000000u | imm26;
	return m_emitter->GetBuffer()->Emit32(op);
}

bool ARM64Relocation::EmitBL(int32_t offset26_instructions) {
	if (m_emitter == nullptr || m_emitter->GetBuffer() == nullptr) {
		return false;
	}
	const uint32_t imm26 = static_cast<uint32_t>(offset26_instructions) & 0x03FFFFFFu;
	const uint32_t op    = 0x94000000u | imm26;
	return m_emitter->GetBuffer()->Emit32(op);
}

bool ARM64Relocation::EmitCBZ(ARM64Reg rt, int32_t offset19_instructions) {
	if (m_emitter == nullptr || m_emitter->GetBuffer() == nullptr) {
		return false;
	}
	const uint32_t imm19 = static_cast<uint32_t>(offset19_instructions) & 0x7FFFFu;
	const uint32_t op    = 0x34000000u | imm19 << 5u | (static_cast<uint32_t>(rt) & 0x1Fu);
	return m_emitter->GetBuffer()->Emit32(op);
}

bool ARM64Relocation::EmitCBNZ(ARM64Reg rt, int32_t offset19_instructions) {
	if (m_emitter == nullptr || m_emitter->GetBuffer() == nullptr) {
		return false;
	}
	const uint32_t imm19 = static_cast<uint32_t>(offset19_instructions) & 0x7FFFFu;
	const uint32_t op    = 0x35000000u | imm19 << 5u | (static_cast<uint32_t>(rt) & 0x1Fu);
	return m_emitter->GetBuffer()->Emit32(op);
}

bool ARM64Relocation::PatchBranch26(uint32_t* instruction_addr, int32_t offset26_instructions) {
	if (instruction_addr == nullptr) {
		return false;
	}
	const uint32_t orig  = *instruction_addr;
	const uint32_t imm26 = static_cast<uint32_t>(offset26_instructions) & 0x03FFFFFFu;
	*instruction_addr    = (orig & 0xFC000000u) | imm26;
	return true;
}

} // namespace Libs::Emulator
