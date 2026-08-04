// x86Decoder.h
//
// Frontend x86-64 Instruction Decoder for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_DECODER_H
#define LOADER_RECOMPILER_X86_DECODER_H

#include "common/common.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace Loader::Recompiler {

enum class X86Opcode : uint16_t {
	Invalid = 0,
	Nop,
	Mov,
	Add,
	Sub,
	Imul,
	And,
	Or,
	Xor,
	Cmp,
	Test,
	Jmp,
	Jcc,
	Call,
	Ret,
	Push,
	Pop
};

enum class X86Condition : uint8_t {
	Overflow = 0, // O / NO
	Carry,        // B / C / NAE
	Equal,        // E / Z
	BelowOrEqual, // BE / NA
	Sign,         // S / NS
	Parity,       // P / PE
	Less,         // L / NGE
	LessOrEqual   // LE / NG
};

enum class X86Reg : uint8_t {
	RAX = 0, RCX, RDX, RBX, RSP, RBP, RSI, RDI,
	R8, R9, R10, R11, R12, R13, R14, R15,
	None = 0xFF
};

struct X86Operand {
	enum class Kind : uint8_t { None, Reg, Imm, Mem } kind = Kind::None;
	X86Reg   reg             = X86Reg::None;
	X86Reg   base_reg        = X86Reg::None;
	X86Reg   index_reg       = X86Reg::None;
	uint8_t  scale           = 1;
	int32_t  disp            = 0;
	int64_t  imm             = 0;
	uint8_t  size_bytes      = 8;

	bool operator==(const X86Operand& other) const = default;
};

struct DecodedX86Instruction {
	X86Opcode    opcode      = X86Opcode::Invalid;
	X86Condition cond        = X86Condition::Equal;
	bool         cond_invert = false;
	X86Operand   dst;
	X86Operand   src;
	uint32_t     length      = 0;
	uint64_t     guest_rip   = 0;
};

class X86Decoder {
public:
	X86Decoder() = default;
	~X86Decoder() = default;

	KYTY_CLASS_NO_COPY(X86Decoder);

	static DecodedX86Instruction DecodeInstruction(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip = 0);
	static std::string DisassembleInstruction(const DecodedX86Instruction& inst);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_DECODER_H
