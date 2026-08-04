// x86Decoder.cpp
//
// Frontend x86-64 Instruction Decoder for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86Decoder.h"
#include "loader/recompiler/x86DecoderTables.h"

#include <cstdio>
#include <sstream>

namespace Loader::Recompiler {

static const OpcodeEntry* GetPrimaryOpcodeEntry(uint8_t opcode) {
	static bool initialized = false;
	static OpcodeEntry table[256];
	if (!initialized) {
		for (int i = 0; i < 256; ++i) table[i] = {X86Opcode::Invalid, OpcodeFormat::Invalid, 0, X86Condition::Equal, false};

		table[0x00] = {X86Opcode::Add, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x01] = {X86Opcode::Add, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x02] = {X86Opcode::Add, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x03] = {X86Opcode::Add, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x04] = {X86Opcode::Add, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x05] = {X86Opcode::Add, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x08] = {X86Opcode::Or, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x09] = {X86Opcode::Or, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x0A] = {X86Opcode::Or, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x0B] = {X86Opcode::Or, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x0C] = {X86Opcode::Or, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x0D] = {X86Opcode::Or, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x0F] = {X86Opcode::Invalid, OpcodeFormat::TwoBytePrefix, 0, X86Condition::Equal, false};

		table[0x10] = {X86Opcode::Adc, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x11] = {X86Opcode::Adc, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x12] = {X86Opcode::Adc, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x13] = {X86Opcode::Adc, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x14] = {X86Opcode::Adc, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x15] = {X86Opcode::Adc, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x18] = {X86Opcode::Sbb, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x19] = {X86Opcode::Sbb, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x1A] = {X86Opcode::Sbb, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x1B] = {X86Opcode::Sbb, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x1C] = {X86Opcode::Sbb, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x1D] = {X86Opcode::Sbb, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x1F] = {X86Opcode::Nop, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};

		table[0x20] = {X86Opcode::And, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x21] = {X86Opcode::And, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x22] = {X86Opcode::And, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x23] = {X86Opcode::And, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x24] = {X86Opcode::And, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x25] = {X86Opcode::And, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x28] = {X86Opcode::Sub, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x29] = {X86Opcode::Sub, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x2A] = {X86Opcode::Sub, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x2B] = {X86Opcode::Sub, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x2C] = {X86Opcode::Sub, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x2D] = {X86Opcode::Sub, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};

		table[0x30] = {X86Opcode::Xor, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x31] = {X86Opcode::Xor, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x32] = {X86Opcode::Xor, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x33] = {X86Opcode::Xor, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x34] = {X86Opcode::Xor, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x35] = {X86Opcode::Xor, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x38] = {X86Opcode::Cmp, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x39] = {X86Opcode::Cmp, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x3A] = {X86Opcode::Cmp, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x3B] = {X86Opcode::Cmp, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x3C] = {X86Opcode::Cmp, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x3D] = {X86Opcode::Cmp, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};

		for (int r = 0; r < 8; ++r) {
			table[0x50 + r] = {X86Opcode::Push, OpcodeFormat::RegEmbed, 0, X86Condition::Equal, false};
			table[0x58 + r] = {X86Opcode::Pop, OpcodeFormat::RegEmbed, 0, X86Condition::Equal, false};
			table[0xB0 + r] = {X86Opcode::Mov, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
			table[0xB8 + r] = {X86Opcode::Mov, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		}

		table[0x63] = {X86Opcode::Movsx, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x68] = {X86Opcode::Push, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};
		table[0x69] = {X86Opcode::Imul, OpcodeFormat::ModRm_Reg_Imm, 4, X86Condition::Equal, false};
		table[0x6A] = {X86Opcode::Push, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0x6B] = {X86Opcode::Imul, OpcodeFormat::ModRm_Reg_Imm, 1, X86Condition::Equal, false};

		static const X86Condition conds[8] = {
			X86Condition::Overflow, X86Condition::Carry, X86Condition::Equal, X86Condition::BelowOrEqual,
			X86Condition::Sign, X86Condition::Parity, X86Condition::Less, X86Condition::LessOrEqual
		};

		for (int c = 0; c < 16; ++c) {
			table[0x70 + c] = {X86Opcode::Jcc, OpcodeFormat::RelImm, 1, conds[c >> 1], (c & 1) != 0};
		}

		table[0x80] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 1, X86Condition::Equal, false};
		table[0x81] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 4, X86Condition::Equal, false};
		table[0x83] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 1, X86Condition::Equal, false};
		table[0x84] = {X86Opcode::Test, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x85] = {X86Opcode::Test, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x88] = {X86Opcode::Mov, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x89] = {X86Opcode::Mov, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x8A] = {X86Opcode::Mov, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x8B] = {X86Opcode::Mov, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x8D] = {X86Opcode::Lea, OpcodeFormat::ModRm_Reg_Mem, 0, X86Condition::Equal, false};
		table[0x8F] = {X86Opcode::Pop, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};

		table[0x90] = {X86Opcode::Nop, OpcodeFormat::NoOperands, 0, X86Condition::Equal, false};
		table[0xA8] = {X86Opcode::Test, OpcodeFormat::RegEmbed, 1, X86Condition::Equal, false};
		table[0xA9] = {X86Opcode::Test, OpcodeFormat::RegEmbed, 4, X86Condition::Equal, false};

		table[0xC0] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 1, X86Condition::Equal, false};
		table[0xC1] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 1, X86Condition::Equal, false};
		table[0xC2] = {X86Opcode::Ret, OpcodeFormat::RegEmbed, 2, X86Condition::Equal, false};
		table[0xC3] = {X86Opcode::Ret, OpcodeFormat::NoOperands, 0, X86Condition::Equal, false};
		table[0xC4] = {X86Opcode::Vex3Byte, OpcodeFormat::VexPrefix3Byte, 0, X86Condition::Equal, false};
		table[0xC5] = {X86Opcode::Vex2Byte, OpcodeFormat::VexPrefix2Byte, 0, X86Condition::Equal, false};
		table[0xC6] = {X86Opcode::Mov, OpcodeFormat::ModRm_Rm_Imm, 1, X86Condition::Equal, false};
		table[0xC7] = {X86Opcode::Mov, OpcodeFormat::ModRm_Rm_Imm, 4, X86Condition::Equal, false};
		table[0xCA] = {X86Opcode::Ret, OpcodeFormat::RegEmbed, 2, X86Condition::Equal, false};
		table[0xCB] = {X86Opcode::Ret, OpcodeFormat::NoOperands, 0, X86Condition::Equal, false};

		table[0xD0] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};
		table[0xD1] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};
		table[0xD2] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};
		table[0xD3] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};

		table[0xE8] = {X86Opcode::Call, OpcodeFormat::RelImm, 4, X86Condition::Equal, false};
		table[0xE9] = {X86Opcode::Jmp, OpcodeFormat::RelImm, 4, X86Condition::Equal, false};
		table[0xEB] = {X86Opcode::Jmp, OpcodeFormat::RelImm, 1, X86Condition::Equal, false};

		table[0xF6] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 1, X86Condition::Equal, false};
		table[0xF7] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 4, X86Condition::Equal, false};
		table[0xFE] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};
		table[0xFF] = {X86Opcode::Invalid, OpcodeFormat::ModRm_Group, 0, X86Condition::Equal, false};

		initialized = true;
	}
	return &table[opcode];
}

static const OpcodeEntry* GetTwoByteOpcodeEntry(uint8_t opcode) {
	static bool initialized = false;
	static OpcodeEntry table[256];
	if (!initialized) {
		for (int i = 0; i < 256; ++i) table[i] = {X86Opcode::Invalid, OpcodeFormat::Invalid, 0, X86Condition::Equal, false};

		table[0x10] = {X86Opcode::Movups, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x11] = {X86Opcode::Movups, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};
		table[0x28] = {X86Opcode::Movaps, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x29] = {X86Opcode::Movaps, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};

		static const X86Condition conds[8] = {
			X86Condition::Overflow, X86Condition::Carry, X86Condition::Equal, X86Condition::BelowOrEqual,
			X86Condition::Sign, X86Condition::Parity, X86Condition::Less, X86Condition::LessOrEqual
		};

		for (int c = 0; c < 16; ++c) {
			table[0x40 + c] = {X86Opcode::Cmov, OpcodeFormat::ModRm_Reg_Rm, 0, conds[c >> 1], (c & 1) != 0};
			table[0x80 + c] = {X86Opcode::Jcc, OpcodeFormat::RelImm, 4, conds[c >> 1], (c & 1) != 0};
			table[0x90 + c] = {X86Opcode::Setcc, OpcodeFormat::ModRm_Rm_Reg, 0, conds[c >> 1], (c & 1) != 0};
		}

		table[0x58] = {X86Opcode::Addps, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x59] = {X86Opcode::Mulps, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x5C] = {X86Opcode::Subps, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x5E] = {X86Opcode::Divps, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x6F] = {X86Opcode::Movdqa, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0x7F] = {X86Opcode::Movdqa, OpcodeFormat::ModRm_Rm_Reg, 0, X86Condition::Equal, false};

		table[0xAF] = {X86Opcode::Imul, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xB6] = {X86Opcode::Movzx, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xB7] = {X86Opcode::Movzx, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xBE] = {X86Opcode::Movsx, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xBF] = {X86Opcode::Movsx, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xDB] = {X86Opcode::Pand, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xEB] = {X86Opcode::Por, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xEF] = {X86Opcode::Pxor, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xFA] = {X86Opcode::Psubd, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};
		table[0xFE] = {X86Opcode::Paddd, OpcodeFormat::ModRm_Reg_Rm, 0, X86Condition::Equal, false};

		initialized = true;
	}
	return &table[opcode];
}

// Group Tables
const X86Opcode g_group1_table[8] = {
	X86Opcode::Add, X86Opcode::Or, X86Opcode::Adc, X86Opcode::Sbb,
	X86Opcode::And, X86Opcode::Sub, X86Opcode::Xor, X86Opcode::Cmp
};

const X86Opcode g_group2_table[8] = {
	X86Opcode::Rol, X86Opcode::Ror, X86Opcode::Invalid, X86Opcode::Invalid,
	X86Opcode::Shl, X86Opcode::Shr, X86Opcode::Shl, X86Opcode::Sar
};

const X86Opcode g_group3_table[8] = {
	X86Opcode::Test, X86Opcode::Test, X86Opcode::Not, X86Opcode::Neg,
	X86Opcode::Mul, X86Opcode::Imul, X86Opcode::Div, X86Opcode::Idiv
};

const X86Opcode g_group5_table[8] = {
	X86Opcode::Inc, X86Opcode::Dec, X86Opcode::Call, X86Opcode::Invalid,
	X86Opcode::Jmp, X86Opcode::Invalid, X86Opcode::Push, X86Opcode::Invalid
};

DecodedX86Instruction X86Decoder::DecodeInstruction(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip) {
	DecodedX86Instruction inst{};
	inst.guest_rip = guest_rip;

	if (!code_ptr || max_bytes == 0) return inst;

	size_t offset = 0;

	// 1. Prefix Parsing Loop
	while (offset < max_bytes) {
		uint8_t byte = code_ptr[offset];

		if (byte >= 0x40 && byte <= 0x4F) { // REX prefix
			inst.has_rex = true;
			inst.rex_w   = (byte & 0x08) != 0;
			inst.rex_r   = (byte & 0x04) != 0;
			inst.rex_x   = (byte & 0x02) != 0;
			inst.rex_b   = (byte & 0x01) != 0;
			offset++;
			break;
		} else if (byte == 0x66) {
			inst.operand_size_override = true;
			offset++;
		} else if (byte == 0x67) {
			inst.address_size_override = true;
			offset++;
		} else if (byte == 0xF0 || byte == 0xF2 || byte == 0xF3) {
			offset++; // Lock / REP prefixes
		} else {
			break;
		}
	}

	if (offset >= max_bytes) return inst;

	// 2. VEX Prefix Check (0xC5 2-byte, 0xC4 3-byte)
	uint8_t opcode_byte = code_ptr[offset++];

	if (opcode_byte == 0xC5 && offset < max_bytes) { // 2-byte VEX
		inst.has_vex = true;
		inst.opcode  = X86Opcode::Vex2Byte;
		uint8_t v1   = code_ptr[offset++];
		inst.rex_r   = (v1 & 0x80) == 0;
		inst.vex_vvvv = (~v1 >> 3) & 0x0F;
		inst.vex_l   = (v1 >> 2) & 0x01;
		inst.vex_pp  = v1 & 0x03;
		inst.is_unsupported = true;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	} else if (opcode_byte == 0xC4 && offset + 1 < max_bytes) { // 3-byte VEX
		inst.has_vex = true;
		inst.opcode  = X86Opcode::Vex3Byte;
		uint8_t v1   = code_ptr[offset++];
		uint8_t v2   = code_ptr[offset++];
		inst.rex_r   = (v1 & 0x80) == 0;
		inst.rex_x   = (v1 & 0x40) == 0;
		inst.rex_b   = (v1 & 0x20) == 0;
		inst.vex_m_mmmm = v1 & 0x1F;
		inst.rex_w   = (v2 & 0x80) != 0;
		inst.vex_vvvv = (~v2 >> 3) & 0x0F;
		inst.vex_l   = (v2 >> 2) & 0x01;
		inst.vex_pp  = v2 & 0x03;
		inst.is_unsupported = true;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// 3. Opcode Table Lookup
	bool is_twobyte = false;
	if (opcode_byte == 0x0F && offset < max_bytes) {
		is_twobyte = true;
		opcode_byte = code_ptr[offset++];
	}

	const OpcodeEntry* entry_ptr = is_twobyte ? GetTwoByteOpcodeEntry(opcode_byte)
	                                         : GetPrimaryOpcodeEntry(opcode_byte);
	const OpcodeEntry& entry = *entry_ptr;

	inst.opcode      = entry.opcode;
	inst.cond        = entry.cond;
	inst.cond_invert = entry.cond_invert;

	// Determine Operand Byte Size
	uint8_t op_size_bytes = 4;
	if (inst.rex_w) {
		op_size_bytes = 8;
	} else if (inst.operand_size_override) {
		op_size_bytes = 2;
	}

	// Format Parsing
	switch (entry.format) {
		case OpcodeFormat::NoOperands:
			break;

		case OpcodeFormat::RegEmbed: {
			uint8_t reg_id = (opcode_byte & 0x07) | (inst.rex_b ? 8 : 0);
			inst.dst.kind = X86Operand::Kind::Reg;
			inst.dst.reg = static_cast<X86Reg>(reg_id);
			inst.dst.size_bytes = op_size_bytes;

			if (entry.default_imm_size > 0 && offset + entry.default_imm_size <= max_bytes) {
				int64_t imm_val = 0;
				size_t read_bytes = (inst.rex_w && entry.default_imm_size == 4) ? 8 : entry.default_imm_size;
				if (offset + read_bytes <= max_bytes) {
					for (size_t i = 0; i < read_bytes; ++i) {
						imm_val |= (static_cast<uint64_t>(code_ptr[offset + i]) << (i * 8));
					}
					offset += read_bytes;
					inst.src.kind = X86Operand::Kind::Imm;
					inst.src.imm = (read_bytes == 1) ? static_cast<int8_t>(imm_val) :
					               (read_bytes == 2) ? static_cast<int16_t>(imm_val) :
					               (read_bytes == 4) ? static_cast<int32_t>(imm_val) : imm_val;
				}
			}
			break;
		}

		case OpcodeFormat::ModRm_Reg_Rm:
		case OpcodeFormat::ModRm_Rm_Reg:
		case OpcodeFormat::ModRm_Reg_Mem:
		case OpcodeFormat::ModRm_Rm_Imm:
		case OpcodeFormat::ModRm_Reg_Imm:
		case OpcodeFormat::ModRm_Group: {
			if (offset >= max_bytes) break;

			uint8_t modrm = code_ptr[offset++];
			uint8_t mod   = (modrm >> 6) & 0x03;
			uint8_t reg   = ((modrm >> 3) & 0x07) | (inst.rex_r ? 8 : 0);
			uint8_t rm    = (modrm & 0x07) | (inst.rex_b ? 8 : 0);

			if (entry.format == OpcodeFormat::ModRm_Group) {
				uint8_t group_op = (modrm >> 3) & 0x07;
				if (opcode_byte == 0x80 || opcode_byte == 0x81 || opcode_byte == 0x83) {
					inst.opcode = g_group1_table[group_op];
				} else if (opcode_byte == 0xC0 || opcode_byte == 0xC1 || opcode_byte == 0xD0 ||
				           opcode_byte == 0xD1 || opcode_byte == 0xD2 || opcode_byte == 0xD3) {
					inst.opcode = g_group2_table[group_op];
				} else if (opcode_byte == 0xF6 || opcode_byte == 0xF7) {
					inst.opcode = g_group3_table[group_op];
				} else if (opcode_byte == 0xFE || opcode_byte == 0xFF) {
					inst.opcode = g_group5_table[group_op];
				}
			}

			// Reg operand
			X86Operand reg_op{};
			reg_op.kind = X86Operand::Kind::Reg;
			reg_op.reg = static_cast<X86Reg>(reg);
			reg_op.size_bytes = op_size_bytes;

			// Rm operand (Reg or Mem)
			X86Operand rm_op{};
			rm_op.size_bytes = op_size_bytes;

			if (mod == 3) {
				rm_op.kind = X86Operand::Kind::Reg;
				rm_op.reg = static_cast<X86Reg>(rm);
			} else {
				rm_op.kind = X86Operand::Kind::Mem;
				uint8_t base_id = modrm & 0x07;

				// Check SIB byte (base == 4)
				if (base_id == 4 && offset < max_bytes) {
					uint8_t sib = code_ptr[offset++];
					uint8_t sib_scale = 1 << ((sib >> 6) & 0x03);
					uint8_t sib_index = ((sib >> 3) & 0x07) | (inst.rex_x ? 8 : 0);
					uint8_t sib_base  = (sib & 0x07) | (inst.rex_b ? 8 : 0);

					rm_op.scale = sib_scale;
					if (sib_index != 4 || inst.rex_x) {
						rm_op.index_reg = static_cast<X86Reg>(sib_index);
					}

					if ((sib & 0x07) == 5 && mod == 0) {
						rm_op.base_reg = X86Reg::None; // No base, disp32 follows
					} else {
						rm_op.base_reg = static_cast<X86Reg>(sib_base);
					}
				} else if (base_id == 5 && mod == 0) { // RIP-relative
					rm_op.is_rip_relative = true;
					rm_op.base_reg = X86Reg::None;
				} else {
					rm_op.base_reg = static_cast<X86Reg>(rm);
				}

				// Displacement parsing
				if (mod == 1 && offset < max_bytes) {
					rm_op.disp = static_cast<int8_t>(code_ptr[offset++]);
				} else if (mod == 2 || (mod == 0 && (base_id == 5 || (base_id == 4 && (rm_op.base_reg == X86Reg::None))))) {
					if (offset + 4 <= max_bytes) {
						int32_t disp32 = 0;
						for (int i = 0; i < 4; ++i) disp32 |= (static_cast<uint32_t>(code_ptr[offset + i]) << (i * 8));
						offset += 4;
						rm_op.disp = disp32;
					}
				}
			}

			// Assign dst/src based on OpcodeFormat
			if (entry.format == OpcodeFormat::ModRm_Reg_Rm || entry.format == OpcodeFormat::ModRm_Reg_Mem) {
				inst.dst = reg_op;
				inst.src = rm_op;
			} else if (entry.format == OpcodeFormat::ModRm_Rm_Reg) {
				inst.dst = rm_op;
				inst.src = reg_op;
			} else if (entry.format == OpcodeFormat::ModRm_Rm_Imm || entry.format == OpcodeFormat::ModRm_Group) {
				inst.dst = rm_op;
				if (entry.default_imm_size > 0) {
					size_t imm_bytes = entry.default_imm_size;
					if (offset + imm_bytes <= max_bytes) {
						int64_t imm_val = 0;
						for (size_t i = 0; i < imm_bytes; ++i) imm_val |= (static_cast<uint64_t>(code_ptr[offset + i]) << (i * 8));
						offset += imm_bytes;
						inst.src.kind = X86Operand::Kind::Imm;
						inst.src.imm = (imm_bytes == 1) ? static_cast<int8_t>(imm_val) : static_cast<int32_t>(imm_val);
					}
				}
			} else if (entry.format == OpcodeFormat::ModRm_Reg_Imm) {
				inst.dst = reg_op;
				inst.src = rm_op;
				if (entry.default_imm_size > 0 && offset + entry.default_imm_size <= max_bytes) {
					int64_t imm_val = 0;
					for (size_t i = 0; i < entry.default_imm_size; ++i) imm_val |= (static_cast<uint64_t>(code_ptr[offset + i]) << (i * 8));
					offset += entry.default_imm_size;
					inst.src2.kind = X86Operand::Kind::Imm;
					inst.src2.imm = (entry.default_imm_size == 1) ? static_cast<int8_t>(imm_val) : static_cast<int32_t>(imm_val);
				}
			}
			break;
		}

		case OpcodeFormat::RelImm: {
			size_t imm_bytes = entry.default_imm_size;
			if (offset + imm_bytes <= max_bytes) {
				int64_t rel_val = 0;
				for (size_t i = 0; i < imm_bytes; ++i) rel_val |= (static_cast<uint64_t>(code_ptr[offset + i]) << (i * 8));
				offset += imm_bytes;
				inst.dst.kind = X86Operand::Kind::Imm;
				inst.dst.imm = (imm_bytes == 1) ? static_cast<int8_t>(rel_val) : static_cast<int32_t>(rel_val);
			}
			break;
		}

		default:
			break;
	}

	inst.length = static_cast<uint32_t>(offset);
	return inst;
}

std::string X86Decoder::DisassembleInstruction(const DecodedX86Instruction& inst) {
	std::stringstream ss;
	switch (inst.opcode) {
		case X86Opcode::Nop:    ss << "nop"; break;
		case X86Opcode::Mov:    ss << "mov"; break;
		case X86Opcode::Movsx:  ss << "movsx"; break;
		case X86Opcode::Movzx:  ss << "movzx"; break;
		case X86Opcode::Lea:    ss << "lea"; break;
		case X86Opcode::Add:    ss << "add"; break;
		case X86Opcode::Adc:    ss << "adc"; break;
		case X86Opcode::Sub:    ss << "sub"; break;
		case X86Opcode::Sbb:    ss << "sbb"; break;
		case X86Opcode::Inc:    ss << "inc"; break;
		case X86Opcode::Dec:    ss << "dec"; break;
		case X86Opcode::Imul:   ss << "imul"; break;
		case X86Opcode::Mul:    ss << "mul"; break;
		case X86Opcode::Idiv:   ss << "idiv"; break;
		case X86Opcode::Div:    ss << "div"; break;
		case X86Opcode::And:    ss << "and"; break;
		case X86Opcode::Or:     ss << "or"; break;
		case X86Opcode::Xor:    ss << "xor"; break;
		case X86Opcode::Not:    ss << "not"; break;
		case X86Opcode::Neg:    ss << "neg"; break;
		case X86Opcode::Cmp:    ss << "cmp"; break;
		case X86Opcode::Test:   ss << "test"; break;
		case X86Opcode::Shl:    ss << "shl"; break;
		case X86Opcode::Shr:    ss << "shr"; break;
		case X86Opcode::Sar:    ss << "sar"; break;
		case X86Opcode::Rol:    ss << "rol"; break;
		case X86Opcode::Ror:    ss << "ror"; break;
		case X86Opcode::Jmp:    ss << "jmp"; break;
		case X86Opcode::Jcc:    ss << "jcc"; break;
		case X86Opcode::Call:   ss << "call"; break;
		case X86Opcode::Ret:    ss << "ret"; break;
		case X86Opcode::Push:   ss << "push"; break;
		case X86Opcode::Pop:    ss << "pop"; break;
		case X86Opcode::Cmov:   ss << "cmovcc"; break;
		case X86Opcode::Setcc:  ss << "setcc"; break;
		case X86Opcode::Movaps: ss << "movaps"; break;
		case X86Opcode::Movups: ss << "movups"; break;
		case X86Opcode::Movdqa: ss << "movdqa"; break;
		case X86Opcode::Movdqu: ss << "movdqu"; break;
		case X86Opcode::Addps:  ss << "addps"; break;
		case X86Opcode::Addpd:  ss << "addpd"; break;
		case X86Opcode::Subps:  ss << "subps"; break;
		case X86Opcode::Subpd:  ss << "subpd"; break;
		case X86Opcode::Mulps:  ss << "mulps"; break;
		case X86Opcode::Mulpd:  ss << "mulpd"; break;
		case X86Opcode::Divps:  ss << "divps"; break;
		case X86Opcode::Divpd:  ss << "divpd"; break;
		case X86Opcode::Paddd:  ss << "paddd"; break;
		case X86Opcode::Psubd:  ss << "psubd"; break;
		case X86Opcode::Pxor:   ss << "pxor"; break;
		case X86Opcode::Pand:   ss << "pand"; break;
		case X86Opcode::Por:    ss << "por"; break;
		case X86Opcode::Vex2Byte: ss << "vex2"; break;
		case X86Opcode::Vex3Byte: ss << "vex3"; break;
		default:                ss << "unknown"; break;
	}
	return ss.str();
}

} // namespace Loader::Recompiler
