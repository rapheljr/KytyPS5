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
	Movsx,
	Movzx,
	Lea,
	Add,
	Adc,
	Sub,
	Sbb,
	Inc,
	Dec,
	Imul,
	Mul,
	Idiv,
	Div,
	And,
	Or,
	Xor,
	Not,
	Neg,
	Cmp,
	Test,
	Shl,
	Shr,
	Sar,
	Rol,
	Ror,
	Bt,
	Bts,
	Btc,
	Bsf,
	Bsr,
	Jmp,
	Jcc,
	Loop,
	Call,
	Ret,
	Push,
	Pop,
	Cmov,
	Setcc,
	// X87 Floating Point Unit (FPU) Opcodes
	Fld,
	Fstp,
	Fadd,
	Fsub,
	Fmul,
	Fdiv,
	// SSE2 Vector & Scalar Opcodes
	Movaps,
	Movups,
	Movdqa,
	Movdqu,
	Addps,
	Addpd,
	Subps,
	Subpd,
	Mulps,
	Mulpd,
	Divps,
	Divpd,
	Paddd,
	Psubd,
	Pxor,
	Pand,
	Por,
	Sqrtps,
	Rsqrtps,
	Andps,
	Andnps,
	Orps,
	Xorps,
	Minps,
	Maxps,
	// SSE3 / SSSE3 / SSE4.1 / SSE4.2 Vector Opcodes
	Haddps,
	Pshufb,
	Pabsd,
	Pmaxsd,
	Pminsd,
	Pblendvb,
	Pcmpestri,
	Pcmpistri,
	// SSE2 Packed Compare & Convert
	Pcmpeqd,
	Pcmpgtd,
	Cvtsi2ss,
	Cvtps2pd,
	Cvtpd2ps,
	// VEX / AVX Opcodes
	Vaddps,
	Vsubps,
	Vmulps,
	Vdivps,
	Vpxor,
	Vex2Byte,
	Vex3Byte,
	// BMI1 / BMI2 / Advanced Bitwise Opcodes
	Popcnt,
	Lzcnt,
	Tzcnt,
	Andn,
	Bextr,
	Blsr,
	Blsmsk,
	Rorx,
	Sarx,
	Shlx,
	Shrx,
	Unsupported
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
	bool     is_rip_relative = false;

	bool operator==(const X86Operand& other) const = default;
};

struct DecodedX86Instruction {
	X86Opcode    opcode                 = X86Opcode::Invalid;
	X86Condition cond                   = X86Condition::Equal;
	bool         cond_invert            = false;
	X86Operand   dst;
	X86Operand   src;
	X86Operand   src2;
	uint32_t     length                 = 0;
	uint64_t     guest_rip              = 0;

	// Prefix & Operand Metadata
	bool         operand_size_override  = false; // 0x66
	bool         address_size_override  = false; // 0x67
	bool         has_lock               = false; // 0xF0
	bool         has_rep                = false; // 0xF2, 0xF3
	bool         has_rex                = false;
	bool         rex_w                  = false;
	bool         rex_r                  = false;
	bool         rex_x                  = false;
	bool         rex_b                  = false;

	// VEX / AVX Metadata
	bool         has_vex                = false;
	uint8_t      vex_l                  = 0;     // Vector length (0 = 128-bit, 1 = 256-bit)
	uint8_t      vex_w                  = 0;
	uint8_t      vex_vvvv               = 0;     // 4-bit inverted register specifier
	uint8_t      vex_m_mmmm             = 0;     // Opcode map selector
	uint8_t      vex_pp                 = 0;     // Mandatory prefix encoding (00=none, 01=66, 10=F3, 11=F2)

	bool         is_unsupported         = false; // Marked true if opcode or VEX sequence is unsupported
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
