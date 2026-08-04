# KytyPS5 Full ARM64 Instruction Selector & Encoder Specification

## Overview
This specification details the instruction encodings, bitfield layouts, immediate optimizations, redundant move eliminations, relocation handling, and pattern-matching instruction selection engine for the **ARM64 Backend** in **KytyPS5**. All instruction encodings strictly adhere to the official ARM Architecture Reference Manual (ARM ARM DDI 0487 / AAPCS64).

---

## 1. Type-Safe Bitfield Encoder Helpers (`Arm64EncoderHelper`)

The encoder replaces raw hexadecimal magic constants with type-safe, constexpr bitfield packers:

| Instruction Family | Encoder Function | Bitfield Structure | Opcode Mask |
| :--- | :--- | :--- | :--- |
| **Data Processing (Imm)** | `AddSubImm` | `sf[31] \| op[30] \| S[29] \| imm12[21:10] \| Rn[9:5] \| Rd[4:0]` | `0x11000000` |
| **Move Wide (Imm)** | `MoveWide` | `sf[31] \| opc[30:29] \| hw[22:21] \| imm16[20:5] \| Rd[4:0]` | `0x12800000` |
| **Logic (Register)** | `LogicReg` | `sf[31] \| opc[30:29] \| N[21] \| Rm[20:16] \| imm6[15:10] \| Rn[9:5] \| Rd[4:0]` | `0x0A000000` |
| **Add/Sub (Register)** | `AddSubReg` | `sf[31] \| op[30] \| S[29] \| Rm[20:16] \| Rn[9:5] \| Rd[4:0]` | `0x0B000000` |
| **Add/Sub Carry** | `AddSubCarry` | `sf[31] \| op[30] \| S[29] \| Rm[20:16] \| Rn[9:5] \| Rd[4:0]` | `0x1A000000` |
| **Multiply / MADD / MSUB** | `Multiply` | `sf[31] \| sub[15] \| Rm[20:16] \| Ra[14:10] \| Rn[9:5] \| Rd[4:0]` | `0x1B000000` |
| **Multiply High (64-bit)** | `MultiplyHigh` | `sf[31] \| signed[23] \| Rm[20:16] \| Rn[9:5] \| Rd[4:0]` | `0x9B407C00` |
| **Variable Shifts** | `ShiftVariable` | `sf[31] \| Rm[20:16] \| opc[11:10] \| Rn[9:5] \| Rd[4:0]` | `0x1AC02000` |
| **Load/Store Unsigned** | `LoadStoreUnsigned` | `size[31:30] \| V[26] \| opc[23:22] \| imm12[21:10] \| Rn[9:5] \| Rt[4:0]` | `0x39000000` |
| **Load/Store Unscaled** | `LoadStoreUnscaled` | `size[31:30] \| V[26] \| opc[23:22] \| simm9[20:12] \| Rn[9:5] \| Rt[4:0]` | `0x38000000` |
| **Load/Store Pair** | `LoadStorePair` | `sf[30] \| L[22] \| simm7[21:15] \| Rt2[14:10] \| Rn[9:5] \| Rt1[4:0]` | `0x29000000` |
| **Unconditional Branch** | `BranchUncond` | `L[31] \| simm26[25:0]` | `0x14000000` |
| **Conditional Branch** | `BranchCond` | `simm19[23:5] \| cond[3:0]` | `0x54000000` |
| **Compare & Branch** | `CompareBranch` | `sf[31] \| op[24] \| simm19[23:5] \| Rt[4:0]` | `0x34000000` |
| **Test & Branch** | `TestBranch` | `b5[31] \| op[24] \| b40[23:19] \| simm14[18:5] \| Rt[4:0]` | `0x36000000` |

---

## 2. Supported Instruction Set Coverage

### Integer Instruction Family
- `AND`: Bitwise AND (`LogicReg opc=0`)
- `ORR`: Bitwise OR (`LogicReg opc=1`)
- `EOR`: Bitwise XOR (`LogicReg opc=2`)
- `MOV`: Register copy (`ORR Xd, XZR, Xm`)
- `MOVZ`: Move zero-wide immediate (`MoveWide opc=2`)
- `MOVK`: Move keep-wide immediate (`MoveWide opc=3`)
- `MOVN`: Move inverted-wide immediate (`MoveWide opc=0`)
- `ADD`: Integer add (`AddSubReg` / `AddSubImm`)
- `SUB`: Integer subtract (`AddSubReg` / `AddSubImm`)
- `ADC`: Add with carry (`AddSubCarry op=0`)
- `SBC`: Subtract with carry (`AddSubCarry op=1`)
- `CMP`: Compare alias (`SUBS XZR, Xn, Xm` / `#imm`)
- `TST`: Bitwise test alias (`ANDS XZR, Xn, Xm`)

### Memory Instruction Family
- `LDR` / `STR`: Unsigned 12-bit scaled offset load/store.
- `LDUR` / `STUR`: Unscaled 9-bit signed offset load/store (`[base, #simm9]`).
- `LDP` / `STP`: 7-bit signed pair load/store (`[base, #simm7]`).

### Branch Instruction Family
- `B`: Unconditional relative branch.
- `BL`: Unconditional branch with link (procedure call).
- `BR`: Register indirect branch.
- `BLR`: Register indirect branch with link.
- `RET`: Return from subroutine (`BR X30`).
- `CBZ` / `CBNZ`: Compare and branch on zero / non-zero.
- `TBZ` / `TBNZ`: Test bit and branch on zero / non-zero.
- `Bcc`: Conditional branch with condition code.

### Shift Instruction Family
- `LSL`: Logical shift left (`ShiftVariable opc=0`)
- `LSR`: Logical shift right (`ShiftVariable opc=1`)
- `ASR`: Arithmetic shift right (`ShiftVariable opc=2`)
- `ROR`: Rotate right (`ShiftVariable opc=3`)

### Multiply Instruction Family
- `MUL`: 64-bit integer multiply (`MADD Xd, Xn, Xm, XZR`)
- `MADD`: Multiply-add (`Xd = Xa + Xn * Xm`)
- `MSUB`: Multiply-subtract (`Xd = Xa - Xn * Xm`)
- `UMULH`: Unsigned 64-bit multiply high result (`UMULH Xd, Xn, Xm`)
- `SMULH`: Signed 64-bit multiply high result (`SMULH Xd, Xn, Xm`)

---

## 3. Optimizations & Instruction Selection Rules

1. **Immediate Optimization**: `EmitMovImm64` inspects 16-bit half-words and emits only necessary `MOVZ`/`MOVK` instructions (skipping zero chunks). Uses `MOVN` for inverted bit patterns.
2. **Redundant Move Elimination**: `EmitMovReg(Xd, Xd)` and `EmitAddImm(Xd, Xd, 0)` automatically suppress instruction emission.
3. **Memory Offset Dispatching**: Automatically selects `LDR`/`STR` for positive aligned offsets and `LDUR`/`STUR` for negative or unaligned offsets.
4. **Pattern-Matching Instruction Selector**: Matches IR nodes to fast control flow (`CBZ`/`CBNZ`/`TBZ`/`TBNZ`) and fused arithmetic (`MADD`/`MSUB`).
