// x86DecoderTables.h
//
// Table-driven Opcode Lookup Definitions for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_DECODER_TABLES_H
#define LOADER_RECOMPILER_X86_DECODER_TABLES_H

#include "loader/recompiler/x86Decoder.h"

#include <cstdint>

namespace Loader::Recompiler {

enum class OpcodeFormat : uint8_t {
	Invalid = 0,
	NoOperands,       // NOP, RET, etc.
	RegEmbed,         // PUSH r64, POP r64, MOV r64, imm
	ModRm_Reg_Rm,     // Dest is Reg (ModRM.reg), Src is Rm (ModRM.rm)
	ModRm_Rm_Reg,     // Dest is Rm (ModRM.rm), Src is Reg (ModRM.reg)
	ModRm_Rm_Imm,     // Dest is Rm (ModRM.rm), Src is Immediate
	ModRm_Reg_Imm,     // Dest is Reg (ModRM.reg), Src is Immediate
	ModRm_Reg_Mem,     // LEA Reg, [Mem]
	ModRm_Group,       // Opcode extended by ModRM.reg field
	RelImm,           // Relative immediate branch (JMP rel32, CALL rel32, Jcc rel8/32)
	TwoBytePrefix,     // 0x0F escape
	VexPrefix2Byte,    // 0xC5 VEX
	VexPrefix3Byte     // 0xC4 VEX
};

struct OpcodeEntry {
	X86Opcode    opcode;
	OpcodeFormat format;
	uint8_t      default_imm_size; // Size in bytes (1, 2, 4, 8) if format uses imm
	X86Condition cond;
	bool         cond_invert;
};

// 1-Byte Primary Opcode Table (0x00 - 0xFF)
extern const OpcodeEntry g_primary_opcode_table[256];

// 2-Byte Opcode Table (0x0F 0x00 - 0x0F 0xFF)
extern const OpcodeEntry g_twobyte_opcode_table[256];

// ModRM Group 1 (0x80, 0x81, 0x83): ADD(0), OR(1), ADC(2), SBB(3), AND(4), SUB(5), XOR(6), CMP(7)
extern const X86Opcode g_group1_table[8];

// ModRM Group 2 (0xC0, 0xC1, 0xD0, 0xD1, 0xD2, 0xD3): ROL(0), ROR(1), RCL(2), RCR(3), SHL(4), SHR(5), SAL(6), SAR(7)
extern const X86Opcode g_group2_table[8];

// ModRM Group 3 (0xF6, 0xF7): TEST(0), TEST(1), NOT(2), NEG(3), MUL(4), IMUL(5), DIV(6), IDIV(7)
extern const X86Opcode g_group3_table[8];

// ModRM Group 4/5 (0xFE, 0xFF): INC(0), DEC(1), CALL(2), CALLF(3), JMP(4), JMPF(5), PUSH(6)
extern const X86Opcode g_group5_table[8];

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_DECODER_TABLES_H
