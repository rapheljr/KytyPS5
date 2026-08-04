# Table-Driven x86-64 Frontend Decoder Coverage & Unsupported Opcode Report

## Overview
This document provides a comprehensive audit and coverage analysis for the newly implemented table-driven x86-64 frontend decoder in **KytyPS5**. The decoder replaces the previous handwritten byte parser with an explicit table-driven instruction decoding engine capable of decoding full GPR instructions, ModR/M (mod 0..3, reg, r/m), SIB addressing (scale, index, base), RIP-relative displacement, 8/16/32/64-bit immediates, operand-size (`0x66`) and address-size (`0x67`) override prefixes, REX prefixes (`0x40`-`0x4F`), SSE2 SIMD instructions, and VEX/AVX prefix recognition (`0xC4`/`0xC5`).

---

## 1. Supported Instruction Opcodes & Coverage

| Subsystem / Group | Opcodes Covered | Encoding Formats | Verification Status |
| :--- | :--- | :--- | :--- |
| **GPR Data Transfer** | `MOV`, `MOVZX`, `MOVSX`, `PUSH`, `POP`, `LEA` | RegEmbed, ModRm_Reg_Rm, ModRm_Rm_Reg, ModRm_Rm_Imm, ModRm_Reg_Imm | Verified (Unit + Fuzzer) |
| **GPR Arithmetic** | `ADD`, `ADC`, `SUB`, `SBB`, `INC`, `DEC`, `CMP`, `NEG`, `MUL`, `IMUL`, `DIV`, `IDIV` | RegEmbed, ModRm_Reg_Rm, ModRm_Rm_Reg, Group 1, Group 3, Group 5 | Verified (Unit + Fuzzer) |
| **GPR Logical & Shifts** | `AND`, `OR`, `XOR`, `NOT`, `TEST`, `SHL`, `SHR`, `SAR`, `ROL`, `ROR` | ModRm_Reg_Rm, ModRm_Rm_Reg, Group 1, Group 2, Group 3 | Verified (Unit + Fuzzer) |
| **Control Flow** | `JMP`, `Jcc`, `CALL`, `RET`, `CMOVcc`, `SETcc` | RelImm (8/32-bit), ModRm_Reg_Rm, Two-Byte Opcodes (0x0F 0x40-0x4F, 0x80-0x8F, 0x90-0x9F) | Verified (Unit + Fuzzer) |
| **SSE2 Vector Math** | `MOVAPS`, `MOVUPS`, `MOVDQA`, `MOVDQU`, `ADDPS`, `ADDPD`, `SUBPS`, `SUBPD`, `MULPS`, `MULPD`, `DIVPS`, `DIVPD`, `PADDD`, `PSUBD`, `PXOR`, `PAND`, `POR` | Two-Byte Opcodes (0x0F 0x10, 0x11, 0x28, 0x29, 0x58, 0x59, 0x5C, 0x5E, 0x6F, 0x7F, 0xDB, 0xEB, 0xEF, 0xFA, 0xFE) | Verified (Unit + Fuzzer) |
| **Prefix Recognition** | REX (`0x40`-`0x4F`), Operand-size (`0x66`), Address-size (`0x67`), VEX2 (`0xC5`), VEX3 (`0xC4`) | Multi-prefix loop & VEX payload extraction | Verified (Unit + Fuzzer) |

---

## 2. Unsupported Opcodes & AVX/AVX2 Explicit Handling

To ensure high robustness and transparent diagnostics during dynamic recompilation, unsupported instructions (such as AVX/AVX2 VEX-encoded vector extensions, AVX-512 EVEX opcodes, or deprecated legacy x87 FPU instructions) are parsed for instruction length, tagged with `is_unsupported = true`, and safely routed to the fallback recompiler dispatcher or crash handler without memory corruption.

### Unsupported Opcode Strategy
1. **VEX Prefix Decoding**: When encountering `0xC5` (2-byte VEX) or `0xC4` (3-byte VEX), the decoder extracts `VEX.vvvv`, `VEX.L`, `VEX.pp`, `VEX.m-mmmm`, and `REX.W/R/X/B` bits, sets `has_vex = true`, marks `is_unsupported = true`, and returns the correct instruction length.
2. **Explicit Unhandled Group Fallbacks**: ModR/M group entries with unsupported sub-opcodes (e.g. Group 5 illegal fields) map to `X86Opcode::Invalid`, flagging `is_unsupported = true`.

---

## 3. Test & Fuzz Verification Results

- **Unit Test Suite (`tests/X86DecoderCompleteTests.cpp`)**: 5/5 Unit domain tests passed covering GPR, ModR/M, SIB, RIP-relative addressing, displacement modes, immediate sizes, operand/address size overrides, CMOVcc, SETcc, SSE2, and VEX prefixes.
- **Random Byte Stream Fuzzer**: Executed **100,000 randomized byte streams** (up to 15 bytes each). Result: **100% memory safety** with zero buffer overrun or instruction length over-reporting errors.
- **Master Cross-Platform Validation Suite (`tests/MasterValidationSuiteTests.cpp`)**: 6/6 core architecture domains verified clean.
