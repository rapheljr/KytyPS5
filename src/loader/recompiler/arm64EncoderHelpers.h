// arm64EncoderHelpers.h
//
// Type-Safe ARM64 Instruction Encoder Helpers & Bitfield Packers (ARM DDI 0487).

#ifndef LOADER_RECOMPILER_ARM64_ENCODER_HELPERS_H
#define LOADER_RECOMPILER_ARM64_ENCODER_HELPERS_H

#include "loader/recompiler/arm64Emitter.h"

#include <cstdint>

namespace Loader::Recompiler::Arm64EncoderHelper {

constexpr inline uint32_t Reg(Arm64Reg r) noexcept {
	return static_cast<uint32_t>(r) & 0x1Fu;
}

// Data Processing (Immediate) - ADD/SUB (imm12)
constexpr inline uint32_t AddSubImm(bool sf, bool is_sub, bool set_flags, uint32_t imm12, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x11000000u;
	if (sf)        inst |= (1u << 31u);
	if (is_sub)    inst |= (1u << 30u);
	if (set_flags) inst |= (1u << 29u);
	inst |= ((imm12 & 0xFFFu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Move Wide (Immediate) - MOVZ, MOVK, MOVN
constexpr inline uint32_t MoveWide(bool sf, uint32_t opc, uint32_t hw, uint32_t imm16, uint32_t rd) noexcept {
	uint32_t inst = 0x12800000u;
	if (sf)        inst |= (1u << 31u);
	inst |= ((opc & 0x03u) << 29u);
	inst |= ((hw & 0x03u) << 21u);
	inst |= ((imm16 & 0xFFFFu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Data Processing (Register) - Logic (AND, ORR, EOR)
constexpr inline uint32_t LogicReg(bool sf, uint32_t opc, bool N, uint32_t rm, uint32_t imm6, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x0A000000u;
	if (sf)     inst |= (1u << 31u);
	inst |= ((opc & 0x03u) << 29u);
	if (N)      inst |= (1u << 21u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((imm6 & 0x3Fu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Data Processing (Register) - Add/Sub (shifted/extended reg)
constexpr inline uint32_t AddSubReg(bool sf, bool is_sub, bool set_flags, uint32_t rm, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x0B000000u;
	if (sf)        inst |= (1u << 31u);
	if (is_sub)    inst |= (1u << 30u);
	if (set_flags) inst |= (1u << 29u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Add/Sub with Carry - ADC, SBC
constexpr inline uint32_t AddSubCarry(bool sf, bool is_sub, bool set_flags, uint32_t rm, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x1A000000u;
	if (sf)        inst |= (1u << 31u);
	if (is_sub)    inst |= (1u << 30u);
	if (set_flags) inst |= (1u << 29u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Multiply & Multiply-Accumulate - MUL, MADD, MSUB, UMULH, SMULH
constexpr inline uint32_t Multiply(bool sf, bool is_sub, uint32_t rm, uint32_t ra, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x1B000000u;
	if (sf)     inst |= (1u << 31u);
	if (is_sub) inst |= (1u << 15u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((ra & 0x1Fu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

constexpr inline uint32_t MultiplyHigh(bool is_signed, uint32_t rm, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x9B407C00u;
	if (is_signed) inst |= (1u << 23u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Shifts - LSL, LSR, ASR, ROR
constexpr inline uint32_t ShiftVariable(bool sf, uint32_t opc, uint32_t rm, uint32_t rn, uint32_t rd) noexcept {
	uint32_t inst = 0x1AC02000u;
	if (sf) inst |= (1u << 31u);
	inst |= ((rm & 0x1Fu) << 16u);
	inst |= ((opc & 0x03u) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

// Load/Store Unsigned Scaled Offset - LDR, STR
constexpr inline uint32_t LoadStoreUnsigned(bool sf, bool is_load, uint32_t imm12, uint32_t rn, uint32_t rt) noexcept {
	uint32_t inst = 0x39000000u;
	if (sf)      inst |= (1u << 30u);
	if (is_load) inst |= (1u << 22u);
	inst |= ((imm12 & 0xFFFu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rt & 0x1Fu);
	return inst;
}

// Load/Store Unscaled Signed Offset - LDUR, STUR
constexpr inline uint32_t LoadStoreUnscaled(bool sf, bool is_load, int32_t simm9, uint32_t rn, uint32_t rt) noexcept {
	uint32_t inst = 0x38000000u;
	if (sf)      inst |= (1u << 30u);
	if (is_load) inst |= (1u << 22u);
	inst |= ((static_cast<uint32_t>(simm9) & 0x1FFu) << 12u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rt & 0x1Fu);
	return inst;
}

// Load/Store Pair - LDP, STP (Signed offset, Pre-indexed, Post-indexed)
constexpr inline uint32_t LoadStorePairOffset(bool sf, bool is_load, int32_t simm7, uint32_t rn, uint32_t rt1, uint32_t rt2) noexcept {
	uint32_t inst = is_load ? 0x29400000u : 0x29000000u;
	if (sf) inst |= (0x2u << 30u); // 64-bit: opc = 10 -> 0xA9400000 (LDP) / 0xA9000000 (STP)
	inst |= ((static_cast<uint32_t>(simm7) & 0x7Fu) << 15u);
	inst |= ((rt2 & 0x1Fu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rt1 & 0x1Fu);
	return inst;
}

constexpr inline uint32_t LoadStorePairPreIndex(bool sf, bool is_load, int32_t simm7, uint32_t rn, uint32_t rt1, uint32_t rt2) noexcept {
	uint32_t inst = is_load ? 0x29C00000u : 0x29800000u;
	if (sf) inst |= (0x2u << 30u); // 64-bit: opc = 10 -> 0xA9C00000 (LDP) / 0xA9800000 (STP)
	inst |= ((static_cast<uint32_t>(simm7) & 0x7Fu) << 15u);
	inst |= ((rt2 & 0x1Fu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rt1 & 0x1Fu);
	return inst;
}

constexpr inline uint32_t LoadStorePairPostIndex(bool sf, bool is_load, int32_t simm7, uint32_t rn, uint32_t rt1, uint32_t rt2) noexcept {
	uint32_t inst = is_load ? 0x28C00000u : 0x28800000u;
	if (sf) inst |= (0x2u << 30u); // 64-bit: opc = 10 -> 0xA8C00000 (LDP) / 0xA8800000 (STP)
	inst |= ((static_cast<uint32_t>(simm7) & 0x7Fu) << 15u);
	inst |= ((rt2 & 0x1Fu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rt1 & 0x1Fu);
	return inst;
}

constexpr inline uint32_t LoadStorePair(bool sf, bool is_load, int32_t simm7, uint32_t rn, uint32_t rt1, uint32_t rt2) noexcept {
	return LoadStorePairOffset(sf, is_load, simm7, rn, rt1, rt2);
}

// Branches & Controls - B, BL, BR, BLR, RET, CBZ, CBNZ, TBZ, TBNZ
constexpr inline uint32_t BranchUncond(bool is_link, int32_t simm26) noexcept {
	uint32_t inst = 0x14000000u;
	if (is_link) inst |= (1u << 31u);
	inst |= (static_cast<uint32_t>(simm26) & 0x03FFFFFFu);
	return inst;
}

constexpr inline uint32_t BranchCond(uint32_t cond, int32_t simm19) noexcept {
	uint32_t inst = 0x54000000u;
	inst |= ((static_cast<uint32_t>(simm19) & 0x7FFFFu) << 5u);
	inst |= (cond & 0x0Fu);
	return inst;
}

constexpr inline uint32_t CompareBranch(bool sf, bool is_nonzero, int32_t simm19, uint32_t rt) noexcept {
	uint32_t inst = 0x34000000u;
	if (sf)         inst |= (1u << 31u);
	if (is_nonzero) inst |= (1u << 24u);
	inst |= ((static_cast<uint32_t>(simm19) & 0x7FFFFu) << 5u);
	inst |= (rt & 0x1Fu);
	return inst;
}

constexpr inline uint32_t TestBranch(bool is_nonzero, uint32_t bit_num, int32_t simm14, uint32_t rt) noexcept {
	uint32_t inst = 0x36000000u;
	if (bit_num & 32u) inst |= (1u << 31u);
	if (is_nonzero)    inst |= (1u << 24u);
	inst |= (((bit_num & 31u) & 0x1Fu) << 19u);
	inst |= ((static_cast<uint32_t>(simm14) & 0x3FFFu) << 5u);
	inst |= (rt & 0x1Fu);
	return inst;
}

} // namespace Loader::Recompiler::Arm64EncoderHelper

#endif // LOADER_RECOMPILER_ARM64_ENCODER_HELPERS_H
