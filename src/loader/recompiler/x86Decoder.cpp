// x86Decoder.cpp
//
// Frontend x86-64 Instruction Decoder for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86Decoder.h"

#include <cstdio>
#include <sstream>

namespace Loader::Recompiler {

DecodedX86Instruction X86Decoder::DecodeInstruction(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip) {
	DecodedX86Instruction inst{};
	inst.guest_rip = guest_rip;

	if (!code_ptr || max_bytes == 0) {
		return inst;
	}

	size_t offset = 0;
	bool rex_w = false;
	bool rex_r = false;
	bool rex_x = false;
	bool rex_b = false;

	// Parse legacy prefixes & REX prefix
	while (offset < max_bytes) {
		uint8_t byte = code_ptr[offset];
		if (byte >= 0x40 && byte <= 0x4F) { // REX prefix
			rex_w = (byte & 0x08) != 0;
			rex_r = (byte & 0x04) != 0;
			rex_x = (byte & 0x02) != 0;
			rex_b = (byte & 0x01) != 0;
			offset++;
			break;
		} else if (byte == 0x66 || byte == 0x67 || byte == 0xF0 || byte == 0xF2 || byte == 0xF3) {
			offset++;
		} else {
			break;
		}
	}

	if (offset >= max_bytes) return inst;

	uint8_t opcode_byte = code_ptr[offset++];

	// NOP (0x90)
	if (opcode_byte == 0x90) {
		inst.opcode = X86Opcode::Nop;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// RET (0xC3)
	if (opcode_byte == 0xC3) {
		inst.opcode = X86Opcode::Ret;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// MOV reg, imm (0xB8 + reg)
	if (opcode_byte >= 0xB8 && opcode_byte <= 0xBF) {
		inst.opcode = X86Opcode::Mov;
		uint8_t reg_id = (opcode_byte - 0xB8) + (rex_b ? 8 : 0);
		inst.dst.kind = X86Operand::Kind::Reg;
		inst.dst.reg = static_cast<X86Reg>(reg_id);

		if (rex_w && (offset + 8 <= max_bytes)) {
			uint64_t imm64 = 0;
			for (int i = 0; i < 8; ++i) imm64 |= (static_cast<uint64_t>(code_ptr[offset + i]) << (i * 8));
			offset += 8;
			inst.src.kind = X86Operand::Kind::Imm;
			inst.src.imm = static_cast<int64_t>(imm64);
		} else if (offset + 4 <= max_bytes) {
			uint32_t imm32 = 0;
			for (int i = 0; i < 4; ++i) imm32 |= (static_cast<uint32_t>(code_ptr[offset + i]) << (i * 8));
			offset += 4;
			inst.src.kind = X86Operand::Kind::Imm;
			inst.src.imm = static_cast<int32_t>(imm32);
		}
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// PUSH reg (0x50 + reg) / POP reg (0x58 + reg)
	if (opcode_byte >= 0x50 && opcode_byte <= 0x57) {
		inst.opcode = X86Opcode::Push;
		inst.dst.kind = X86Operand::Kind::Reg;
		inst.dst.reg = static_cast<X86Reg>((opcode_byte - 0x50) + (rex_b ? 8 : 0));
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}
	if (opcode_byte >= 0x58 && opcode_byte <= 0x5F) {
		inst.opcode = X86Opcode::Pop;
		inst.dst.kind = X86Operand::Kind::Reg;
		inst.dst.reg = static_cast<X86Reg>((opcode_byte - 0x58) + (rex_b ? 8 : 0));
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// JMP rel32 (0xE9) / CALL rel32 (0xE8)
	if ((opcode_byte == 0xE9 || opcode_byte == 0xE8) && (offset + 4 <= max_bytes)) {
		inst.opcode = (opcode_byte == 0xE9) ? X86Opcode::Jmp : X86Opcode::Call;
		int32_t rel32 = 0;
		for (int i = 0; i < 4; ++i) rel32 |= (static_cast<uint32_t>(code_ptr[offset + i]) << (i * 8));
		offset += 4;
		inst.dst.kind = X86Operand::Kind::Imm;
		inst.dst.imm = rel32;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// JCC rel8 (0x70 - 0x7F)
	if (opcode_byte >= 0x70 && opcode_byte <= 0x7F && offset + 1 <= max_bytes) {
		inst.opcode = X86Opcode::Jcc;
		inst.cond = static_cast<X86Condition>((opcode_byte & 0x0E) >> 1);
		inst.cond_invert = (opcode_byte & 0x01) != 0;
		int8_t rel8 = static_cast<int8_t>(code_ptr[offset++]);
		inst.dst.kind = X86Operand::Kind::Imm;
		inst.dst.imm = rel8;
		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	// ModR/M decoding for standard two-operand instructions (MOV, ADD, SUB, CMP, XOR, AND, OR)
	if (offset < max_bytes) {
		uint8_t modrm = code_ptr[offset++];
		uint8_t mod = (modrm >> 6) & 0x03;
		uint8_t reg = ((modrm >> 3) & 0x07) + (rex_r ? 8 : 0);
		uint8_t rm  = (modrm & 0x07) + (rex_b ? 8 : 0);

		if (opcode_byte == 0x8B || opcode_byte == 0x89) { // MOV reg, r/m or MOV r/m, reg
			inst.opcode = X86Opcode::Mov;
			if (opcode_byte == 0x8B) {
				inst.dst.kind = X86Operand::Kind::Reg;
				inst.dst.reg = static_cast<X86Reg>(reg);
				if (mod == 3) {
					inst.src.kind = X86Operand::Kind::Reg;
					inst.src.reg = static_cast<X86Reg>(rm);
				}
			} else {
				if (mod == 3) {
					inst.dst.kind = X86Operand::Kind::Reg;
					inst.dst.reg = static_cast<X86Reg>(rm);
				}
				inst.src.kind = X86Operand::Kind::Reg;
				inst.src.reg = static_cast<X86Reg>(reg);
			}
		} else if (opcode_byte == 0x01 || opcode_byte == 0x03) { // ADD
			inst.opcode = X86Opcode::Add;
			inst.dst.kind = X86Operand::Kind::Reg;
			inst.dst.reg = static_cast<X86Reg>((opcode_byte == 0x03) ? reg : rm);
			inst.src.kind = X86Operand::Kind::Reg;
			inst.src.reg = static_cast<X86Reg>((opcode_byte == 0x03) ? rm : reg);
		} else if (opcode_byte == 0x29 || opcode_byte == 0x2B) { // SUB
			inst.opcode = X86Opcode::Sub;
			inst.dst.kind = X86Operand::Kind::Reg;
			inst.dst.reg = static_cast<X86Reg>((opcode_byte == 0x2B) ? reg : rm);
			inst.src.kind = X86Operand::Kind::Reg;
			inst.src.reg = static_cast<X86Reg>((opcode_byte == 0x2B) ? rm : reg);
		} else if (opcode_byte == 0x31 || opcode_byte == 0x33) { // XOR
			inst.opcode = X86Opcode::Xor;
			inst.dst.kind = X86Operand::Kind::Reg;
			inst.dst.reg = static_cast<X86Reg>((opcode_byte == 0x33) ? reg : rm);
			inst.src.kind = X86Operand::Kind::Reg;
			inst.src.reg = static_cast<X86Reg>((opcode_byte == 0x33) ? rm : reg);
		} else if (opcode_byte == 0x39 || opcode_byte == 0x3B) { // CMP
			inst.opcode = X86Opcode::Cmp;
			inst.dst.kind = X86Operand::Kind::Reg;
			inst.dst.reg = static_cast<X86Reg>((opcode_byte == 0x3B) ? reg : rm);
			inst.src.kind = X86Operand::Kind::Reg;
			inst.src.reg = static_cast<X86Reg>((opcode_byte == 0x3B) ? rm : reg);
		}

		inst.length = static_cast<uint32_t>(offset);
		return inst;
	}

	inst.length = static_cast<uint32_t>(offset);
	return inst;
}

std::string X86Decoder::DisassembleInstruction(const DecodedX86Instruction& inst) {
	std::stringstream ss;
	switch (inst.opcode) {
		case X86Opcode::Nop:  ss << "nop"; break;
		case X86Opcode::Mov:  ss << "mov"; break;
		case X86Opcode::Add:  ss << "add"; break;
		case X86Opcode::Sub:  ss << "sub"; break;
		case X86Opcode::Imul: ss << "imul"; break;
		case X86Opcode::And:  ss << "and"; break;
		case X86Opcode::Or:   ss << "or"; break;
		case X86Opcode::Xor:  ss << "xor"; break;
		case X86Opcode::Cmp:  ss << "cmp"; break;
		case X86Opcode::Test: ss << "test"; break;
		case X86Opcode::Jmp:  ss << "jmp"; break;
		case X86Opcode::Jcc:  ss << "jcc"; break;
		case X86Opcode::Call: ss << "call"; break;
		case X86Opcode::Ret:  ss << "ret"; break;
		case X86Opcode::Push: ss << "push"; break;
		case X86Opcode::Pop:  ss << "pop"; break;
		default:              ss << "unknown"; break;
	}
	return ss.str();
}

} // namespace Loader::Recompiler
