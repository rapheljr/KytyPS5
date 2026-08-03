#include "emulator/arm64Emitter.h"

namespace Libs::Emulator {

bool ARM64Emitter::EmitNOP() {
	if (m_buffer == nullptr) {
		return false;
	}
	return m_buffer->Emit32(0xD503201Fu);
}

bool ARM64Emitter::EmitRET(ARM64Reg rn) {
	if (m_buffer == nullptr) {
		return false;
	}
	const uint32_t op = 0xD65F0000u | (static_cast<uint32_t>(rn) & 0x1Fu) << 5u;
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitMOVZ(ARM64Reg rd, uint16_t imm16, uint8_t shift) {
	if (m_buffer == nullptr || (shift % 16 != 0) || shift > 48) {
		return false;
	}
	const uint32_t hw = shift / 16;
	const uint32_t op = 0xD2800000u | (hw & 3u) << 21u | (static_cast<uint32_t>(imm16) & 0xFFFFu) << 5u |
	                    (static_cast<uint32_t>(rd) & 0x1Fu);
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitMOVK(ARM64Reg rd, uint16_t imm16, uint8_t shift) {
	if (m_buffer == nullptr || (shift % 16 != 0) || shift > 48) {
		return false;
	}
	const uint32_t hw = shift / 16;
	const uint32_t op = 0xF2800000u | (hw & 3u) << 21u | (static_cast<uint32_t>(imm16) & 0xFFFFu) << 5u |
	                    (static_cast<uint32_t>(rd) & 0x1Fu);
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitMOV64(ARM64Reg rd, uint64_t imm64) {
	const uint16_t w0 = static_cast<uint16_t>(imm64 & 0xFFFFu);
	const uint16_t w1 = static_cast<uint16_t>((imm64 >> 16u) & 0xFFFFu);
	const uint16_t w2 = static_cast<uint16_t>((imm64 >> 32u) & 0xFFFFu);
	const uint16_t w3 = static_cast<uint16_t>((imm64 >> 48u) & 0xFFFFu);

	if (!EmitMOVZ(rd, w0, 0)) {
		return false;
	}
	if (w1 != 0 && !EmitMOVK(rd, w1, 16)) {
		return false;
	}
	if (w2 != 0 && !EmitMOVK(rd, w2, 32)) {
		return false;
	}
	if (w3 != 0 && !EmitMOVK(rd, w3, 48)) {
		return false;
	}
	return true;
}

bool ARM64Emitter::EmitADD(ARM64Reg rd, ARM64Reg rn, ARM64Reg rm) {
	if (m_buffer == nullptr) {
		return false;
	}
	const uint32_t op = 0x8B000000u | (static_cast<uint32_t>(rm) & 0x1Fu) << 16u |
	                    (static_cast<uint32_t>(rn) & 0x1Fu) << 5u | (static_cast<uint32_t>(rd) & 0x1Fu);
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitSUB(ARM64Reg rd, ARM64Reg rn, ARM64Reg rm) {
	if (m_buffer == nullptr) {
		return false;
	}
	const uint32_t op = 0xCB000000u | (static_cast<uint32_t>(rm) & 0x1Fu) << 16u |
	                    (static_cast<uint32_t>(rn) & 0x1Fu) << 5u | (static_cast<uint32_t>(rd) & 0x1Fu);
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitBLR(ARM64Reg rn) {
	if (m_buffer == nullptr) {
		return false;
	}
	const uint32_t op = 0xD63F0000u | (static_cast<uint32_t>(rn) & 0x1Fu) << 5u;
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitSVC(uint16_t imm16) {
	if (m_buffer == nullptr) {
		return false;
	}
	const uint32_t op = 0xD4000001u | (static_cast<uint32_t>(imm16) & 0xFFFFu) << 5u;
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitSTP_PreIndex(ARM64Reg rt1, ARM64Reg rt2, ARM64Reg rn, int32_t simm7) {
	if (m_buffer == nullptr || simm7 < -64 || simm7 > 63) {
		return false;
	}
	const uint32_t imm7 = static_cast<uint32_t>(simm7) & 0x7Fu;
	const uint32_t op   = 0xA9B80000u | imm7 << 15u | (static_cast<uint32_t>(rt2) & 0x1Fu) << 10u |
	                    (static_cast<uint32_t>(rn) & 0x1Fu) << 5u | (static_cast<uint32_t>(rt1) & 0x1Fu);
	return m_buffer->Emit32(op);
}

bool ARM64Emitter::EmitLDP_PostIndex(ARM64Reg rt1, ARM64Reg rt2, ARM64Reg rn, int32_t simm7) {
	if (m_buffer == nullptr || simm7 < -64 || simm7 > 63) {
		return false;
	}
	const uint32_t imm7 = static_cast<uint32_t>(simm7) & 0x7Fu;
	const uint32_t op   = 0xA8C00000u | imm7 << 15u | (static_cast<uint32_t>(rt2) & 0x1Fu) << 10u |
	                    (static_cast<uint32_t>(rn) & 0x1Fu) << 5u | (static_cast<uint32_t>(rt1) & 0x1Fu);
	return m_buffer->Emit32(op);
}

} // namespace Libs::Emulator
