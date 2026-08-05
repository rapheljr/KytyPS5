// arm64OptimizationPipeline.cpp
//
// ARM64 Post-Lowering Machine-Level Peephole Optimization Pipeline.
//
// Pass execution order (single sweep, fixed sequence):
//   1  MOV Elimination        — identity copies, dead XZR writes
//   2  Zero Register Opt      — AND/ORR/ADD with XZR fold to MOV XZR
//   3  Constant Folding       — MOVZ+ADD #imm → MOVZ(result)
//   4  Immediate Folding      — ADD/SUB #0 → NOP
//   5  Instruction Combining  — LSL+ADD → ADD-shifted encoding
//   6  Register Coalescing    — MOV copy chain collapse
//   7  Load/Store Folding     — STR Xn+LDR Xm same slot → MOV forwarding
//   8  Dead Code Elimination  — writes to registers never subsequently read
//   9  Branch Simplification  — branch-over-NOP → delete NOP
//  10  NOP Compaction         — sweep and rebuild final vector

#include "loader/recompiler/arm64OptimizationPipeline.h"

#include <array>
#include <cassert>
#include <cstdio>
#include <sstream>
#include <unordered_map>

namespace Loader::Recompiler {

// ─── Constants ───────────────────────────────────────────────────────────────

static constexpr uint8_t  kXZR     = 31u;
static constexpr uint32_t kNopWord = 0xD503201Fu;  // ARM64 NOP

// Cycle cost table (conservative estimates for Apple Silicon M-series)
static constexpr uint32_t CycleFor(MachInstKind k) noexcept {
	switch (k) {
		case MachInstKind::Load:
		case MachInstKind::LoadPair:  return 4;
		case MachInstKind::Store:
		case MachInstKind::StorePair: return 2;
		case MachInstKind::Mul:       return 3;
		case MachInstKind::Branch:
		case MachInstKind::BranchLink:
		case MachInstKind::BranchCond:
		case MachInstKind::CmpBranchZ:
		case MachInstKind::CmpBranchNZ: return 1;
		case MachInstKind::Nop:       return 0;
		default:                      return 1;
	}
}

// ─── Decode Helpers ───────────────────────────────────────────────────────────

bool Arm64OptimizationPipeline::IsNop(uint32_t w) noexcept {
	return w == kNopWord;
}

bool Arm64OptimizationPipeline::IsLoad(uint32_t w) noexcept {
	// LDR Xt, [Xn, #imm12*8]:  top 9 bits = 1111_1001_0  (0xF9400000 >> 23 = 0x1F2)
	const uint32_t ldr_mask  = 0xFFC00000u;
	const uint32_t ldr_enc   = 0xF9400000u;
	// LDUR Xt, [Xn, #simm9]:   top 10 bits = 1111_1000_01  (size=11,V=0,opc=01)
	const uint32_t ldur_mask = 0xFFE00C00u;
	const uint32_t ldur_enc  = 0xF8400000u;
	// LDP Xt1,Xt2,[Xn,#imm7]:  top 10 bits = 1010_1001_01
	const uint32_t ldp_mask  = 0xFFC00000u;
	const uint32_t ldp_enc   = 0xA9400000u;
	return (w & ldr_mask)  == ldr_enc
	    || (w & ldur_mask) == ldur_enc
	    || (w & ldp_mask)  == ldp_enc;
}

bool Arm64OptimizationPipeline::IsStore(uint32_t w) noexcept {
	// STR Xt, [Xn, #imm12*8]: top 10 bits = 1111_1001_00
	const uint32_t str_mask  = 0xFFC00000u;
	const uint32_t str_enc   = 0xF9000000u;
	// STUR Xt,[Xn,#simm9]: top 10 bits = 1111_1000_00
	const uint32_t stur_mask = 0xFFE00C00u;
	const uint32_t stur_enc  = 0xF8000000u;
	// STP Xt1,Xt2,[Xn,#imm7]: top 10 bits = 1010_1001_00 or 1010_1001_10
	const uint32_t stp_mask  = 0xFF800000u;
	const uint32_t stp_enc   = 0xA9000000u;
	return (w & str_mask)  == str_enc
	    || (w & stur_mask) == stur_enc
	    || (w & stp_mask)  == stp_enc;
}

bool Arm64OptimizationPipeline::IsBranch(uint32_t w) noexcept {
	uint32_t top8 = w >> 24u;
	return (top8 & 0xFCu) == 0x14u    // B / BL
	    || (w & 0xFF000000u) == 0x54000000u // B.cond
	    || (w & 0x7E000000u) == 0x34000000u // CBZ / CBNZ
	    || (w & 0x7E000000u) == 0x36000000u // TBZ / TBNZ
	    || (w & 0xFFFFFC1Fu) == 0xD61F0000u // BR
	    || (w & 0xFFFFFC1Fu) == 0xD63F0000u // BLR
	    || (w & 0xFFFFF83Fu) == 0xD65F0000u; // RET
}

bool Arm64OptimizationPipeline::IsMovReg(uint32_t w, uint8_t& rd, uint8_t& rn) noexcept {
	// MOV Xd, Xn is encoded as ORR Xd, XZR, Xn (no shift):
	// sf=1, opc=01, N=0, shift=00, imm6=0, Rm, Rn=XZR(11111), Rd
	// Encoding: 1_01_01010_00_0_RRRRR_000000_11111_DDDDD
	if ((w & 0x7F2003E0u) == 0x2A0003E0u) {
		rd = static_cast<uint8_t>(w & 0x1Fu);
		rn = static_cast<uint8_t>((w >> 16u) & 0x1Fu);
		return true;
	}
	return false;
}

bool Arm64OptimizationPipeline::IsMovz(uint32_t w, uint8_t& rd, uint16_t& imm16, uint8_t& hw) noexcept {
	// MOVZ: sf(1) | opc=10(2) | 100101(6) | hw(2) | imm16(16) | Rd(5)
	// Mask off sf, hw, imm16, Rd to match fixed bits: bits[28:23] = 100101
	const uint32_t movz_mask = 0x1F800000u; // bits 28..23
	const uint32_t movz_bits = 0x12800000u; // opc=10, fixed
	if ((w & movz_mask) == movz_bits) {
		rd    = static_cast<uint8_t>(w & 0x1Fu);
		imm16 = static_cast<uint16_t>((w >> 5u) & 0xFFFFu);
		hw    = static_cast<uint8_t>((w >> 21u) & 0x03u);
		return true;
	}
	return false;
}

bool Arm64OptimizationPipeline::HasSideEffects(const Arm64MachineInst& inst) noexcept {
	switch (inst.kind) {
		case MachInstKind::Store:
		case MachInstKind::StorePair:
		case MachInstKind::Branch:
		case MachInstKind::BranchLink:
		case MachInstKind::BranchReg:
		case MachInstKind::BranchCond:
		case MachInstKind::CmpBranchZ:
		case MachInstKind::CmpBranchNZ:
		case MachInstKind::Ret:
			return true;
		default:
			return inst.sets_flags;
	}
}

// ─── Encode Helpers ───────────────────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::EncodeMovReg(uint8_t rd, uint8_t rn, bool sf) noexcept {
	// ORR Xd, XZR, Xm  (sf | 0010_1010_000 | Rm | 000000 | 11111 | Rd)
	uint32_t inst = 0x2A0003E0u;
	if (sf) inst |= (1u << 31u);
	inst |= ((rn & 0x1Fu) << 16u);
	inst |= (rd & 0x1Fu);
	return inst;
}

uint32_t Arm64OptimizationPipeline::EncodeMovz(uint8_t rd, uint16_t imm16, uint8_t hw, bool sf) noexcept {
	uint32_t inst = 0x52800000u;
	if (sf) inst |= (1u << 31u);
	inst |= ((hw & 0x03u) << 21u);
	inst |= ((imm16 & 0xFFFFu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

uint32_t Arm64OptimizationPipeline::EncodeAddImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool sf) noexcept {
	uint32_t inst = 0x11000000u;
	if (sf) inst |= (1u << 31u);
	inst |= ((imm12 & 0xFFFu) << 10u);
	inst |= ((rn & 0x1Fu) << 5u);
	inst |= (rd & 0x1Fu);
	return inst;
}

uint32_t Arm64OptimizationPipeline::EncodeNop() noexcept {
	return kNopWord;
}

// ─── Full Decode ──────────────────────────────────────────────────────────────

Arm64MachineInst Arm64OptimizationPipeline::Decode(uint32_t w) noexcept {
	Arm64MachineInst inst;
	inst.encoding = w;

	if (IsNop(w)) {
		inst.kind = MachInstKind::Nop;
		return inst;
	}

	// MOV Xd, Xn  (ORR Xd, XZR, Xn with no shift)
	{
		uint8_t rd, rn;
		if (IsMovReg(w, rd, rn)) {
			inst.kind = MachInstKind::MovReg;
			inst.rd   = rd;
			inst.rn   = rn;
			inst.sf   = (w >> 31u) & 1u;
			return inst;
		}
	}

	// MOVZ
	{
		uint8_t rd; uint16_t imm16; uint8_t hw;
		if (IsMovz(w, rd, imm16, hw)) {
			inst.kind  = MachInstKind::MovImm;
			inst.rd    = rd;
			inst.imm   = int64_t(imm16) << (hw * 16);
			inst.hw    = hw;
			inst.sf    = (w >> 31u) & 1u;
			return inst;
		}
	}

	// ADD (immediate): sf | 0 | 0 | 10001 | shift(2) | imm12(12) | Rn(5) | Rd(5)
	if ((w & 0x7F000000u) == 0x11000000u) {
		inst.kind = MachInstKind::AddImm;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = (w >> 10u) & 0xFFFu;
		return inst;
	}

	// SUB (immediate)
	if ((w & 0x7F000000u) == 0x51000000u) {
		inst.kind = MachInstKind::SubImm;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = (w >> 10u) & 0xFFFu;
		return inst;
	}

	// ADD (register): sf | 0 | 0 | 01011 | shift(2) | 0 | Rm(5) | imm6(6) | Rn(5) | Rd(5)
	if ((w & 0x7F200000u) == 0x0B000000u) {
		inst.kind = MachInstKind::AddReg;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		inst.imm  = (w >> 10u) & 0x3Fu; // shift amount in imm6
		return inst;
	}

	// SUB (register)
	if ((w & 0x7F200000u) == 0x4B000000u) {
		inst.kind = MachInstKind::SubReg;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		return inst;
	}

	// AND (register)
	if ((w & 0x7F200000u) == 0x0A000000u) {
		inst.kind = MachInstKind::AndReg;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		return inst;
	}

	// ORR (register)
	if ((w & 0x7F200000u) == 0x2A000000u) {
		inst.kind = MachInstKind::OrrReg;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		return inst;
	}

	// EOR (register)
	if ((w & 0x7F200000u) == 0x4A000000u) {
		inst.kind = MachInstKind::EorReg;
		inst.sf   = (w >> 31u) & 1u;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		return inst;
	}

	// UBFM (encodes LSL #shift when imms+1==immr, LSR otherwise)
	// Fixed bits [28:23] = 100110; sf selects 0x53/0xD3 base
	{
		const uint32_t ubfm_mask = 0x1F800000u;
		const uint32_t ubfm_bits = 0x13000000u; // opc=00, fixed
		if ((w & ubfm_mask) == ubfm_bits) {
			uint32_t immr = (w >> 16u) & 0x3Fu;
			uint32_t imms = (w >> 10u) & 0x3Fu;
			inst.rd  = w & 0x1Fu;
			inst.rn  = (w >> 5u) & 0x1Fu;
			inst.sf  = (w >> 31u) & 1u;
			// LSL: imms+1 == immr  (when immr != 0)
			if (immr != 0 && ((imms + 1u) == immr)) {
				inst.kind = MachInstKind::LslImm;
				inst.imm  = 64 - immr;
			} else if (imms == 63 || imms == 31) {
				inst.kind = MachInstKind::LsrImm;
				inst.imm  = immr;
			} else {
				inst.kind = MachInstKind::LslImm;
				inst.imm  = immr;
			}
			return inst;
		}
	}

	// MUL (register) = MADD with Ra=XZR
	// top 9 bits = 1001_1011_0 = 0x136, lower fixed = opc=00, Ra=11111
	if ((w & 0xFFE08000u) == 0x9B000000u && ((w >> 10u) & 0x1Fu) == 0x1Fu) {
		inst.kind = MachInstKind::Mul;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.rm   = (w >> 16u) & 0x1Fu;
		inst.sf   = true;
		return inst;
	}

	// LDR X (unsigned offset): top 10 bits = 1111_1001_01
	if ((w & 0xFFC00000u) == 0xF9400000u) {
		inst.kind = MachInstKind::Load;
		inst.rd   = w & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = int64_t((w >> 10u) & 0xFFFu) * 8;
		inst.sf   = true;
		return inst;
	}

	// STR X (unsigned offset): top 10 bits = 1111_1001_00
	if ((w & 0xFFC00000u) == 0xF9000000u) {
		inst.kind = MachInstKind::Store;
		inst.rd   = w & 0x1Fu;  // Rt (value register)
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = int64_t((w >> 10u) & 0xFFFu) * 8;
		inst.sf   = true;
		return inst;
	}

	// LDP X (signed offset): top 10 bits = 1010_1001_01
	if ((w & 0xFFC00000u) == 0xA9400000u) {
		inst.kind = MachInstKind::LoadPair;
		inst.rd   = w & 0x1Fu;
		inst.rm   = (w >> 10u) & 0x1Fu;  // Rt2
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = int64_t(static_cast<int8_t>((w >> 15u) & 0x7Fu)) * 8;
		return inst;
	}

	// STP X (pre-index or signed-offset): top 9 bits = 1010_1001_0
	if ((w & 0xFF800000u) == 0xA9000000u) {
		inst.kind = MachInstKind::StorePair;
		inst.rd   = w & 0x1Fu;
		inst.rm   = (w >> 10u) & 0x1Fu;
		inst.rn   = (w >> 5u) & 0x1Fu;
		inst.imm  = int64_t(static_cast<int8_t>((w >> 15u) & 0x7Fu)) * 8;
		return inst;
	}

	// B / BL
	if ((w & 0xFC000000u) == 0x14000000u) {
		inst.kind = ((w >> 31u) & 1u) ? MachInstKind::BranchLink : MachInstKind::Branch;
		inst.imm  = int64_t(static_cast<int32_t>(w << 6u) >> 6u); // sign-extended simm26
		return inst;
	}

	// B.cond
	if ((w & 0xFF000000u) == 0x54000000u) {
		inst.kind      = MachInstKind::BranchCond;
		inst.cond_code = w & 0x0Fu;
		inst.imm       = int64_t(static_cast<int32_t>((w & 0x00FFFFE0u)) >> 3u);
		return inst;
	}

	// CBZ
	if ((w & 0x7F000000u) == 0x34000000u) {
		inst.kind = MachInstKind::CmpBranchZ;
		inst.rd   = w & 0x1Fu;
		inst.imm  = int64_t(static_cast<int32_t>((w & 0x00FFFFE0u)) >> 3u);
		return inst;
	}

	// CBNZ
	if ((w & 0x7F000000u) == 0x35000000u) {
		inst.kind = MachInstKind::CmpBranchNZ;
		inst.rd   = w & 0x1Fu;
		inst.imm  = int64_t(static_cast<int32_t>((w & 0x00FFFFE0u)) >> 3u);
		return inst;
	}

	// RET
	if ((w & 0xFFFFF83Fu) == 0xD65F0000u) {
		inst.kind = MachInstKind::Ret;
		inst.rn   = (w >> 5u) & 0x1Fu;
		return inst;
	}

	// BR
	if ((w & 0xFFFFFC1Fu) == 0xD61F0000u) {
		inst.kind = MachInstKind::BranchReg;
		inst.rn   = (w >> 5u) & 0x1Fu;
		return inst;
	}

	inst.kind = MachInstKind::Other;
	return inst;
}

uint32_t Arm64OptimizationPipeline::Encode(const Arm64MachineInst& inst) noexcept {
	switch (inst.kind) {
		case MachInstKind::Nop:    return kNopWord;
		case MachInstKind::MovReg: return EncodeMovReg(inst.rd, inst.rn, inst.sf);
		case MachInstKind::MovImm: return EncodeMovz(inst.rd, static_cast<uint16_t>(inst.imm >> (inst.hw * 16)), inst.hw, inst.sf);
		case MachInstKind::AddImm: return EncodeAddImm(inst.rd, inst.rn, static_cast<uint32_t>(inst.imm), inst.sf);
		default:                   return inst.encoding; // pass through unchanged
	}
}

// ─── Pass 1: MOV Elimination ─────────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassMovElimination(std::vector<Arm64MachineInst>& insts) {
	uint32_t removed = 0;
	for (auto& inst : insts) {
		if (inst.is_dead) continue;
		if (inst.kind == MachInstKind::MovReg) {
			// MOV Xd, Xd  — self-copy
			if (inst.rd == inst.rn) {
				inst.kind    = MachInstKind::Nop;
				inst.encoding = kNopWord;
				inst.is_dead  = true;
				++removed;
			}
			// MOV XZR, Xn  — write to zero register, no effect
			else if (inst.rd == kXZR) {
				inst.kind    = MachInstKind::Nop;
				inst.encoding = kNopWord;
				inst.is_dead  = true;
				++removed;
			}
		}
	}
	return removed;
}

// ─── Pass 2: Zero Register Optimization ──────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassZeroRegisterOpt(std::vector<Arm64MachineInst>& insts) {
	uint32_t changed = 0;
	for (auto& inst : insts) {
		if (inst.is_dead) continue;
		switch (inst.kind) {
			case MachInstKind::AndReg:
				// AND Xd, Xn, XZR → MOV Xd, XZR (zero)
				if (inst.rm == kXZR || inst.rn == kXZR) {
					inst.kind    = MachInstKind::MovReg;
					inst.rn      = kXZR;
					inst.rm      = kXZR;
					inst.encoding = EncodeMovReg(inst.rd, kXZR, inst.sf);
					++changed;
				}
				break;
			case MachInstKind::OrrReg:
				// ORR Xd, XZR, XZR → MOV Xd, XZR
				if (inst.rn == kXZR && inst.rm == kXZR) {
					inst.kind    = MachInstKind::MovReg;
					inst.rn      = kXZR;
					inst.encoding = EncodeMovReg(inst.rd, kXZR, inst.sf);
					++changed;
				}
				break;
			case MachInstKind::AddReg:
				// ADD Xd, XZR, Xm → MOV Xd, Xm
				if (inst.rn == kXZR && inst.imm == 0) {
					inst.kind    = MachInstKind::MovReg;
					inst.rn      = inst.rm;
					inst.encoding = EncodeMovReg(inst.rd, inst.rm, inst.sf);
					++changed;
				}
				// ADD Xd, Xn, XZR → MOV Xd, Xn
				else if (inst.rm == kXZR && inst.imm == 0) {
					inst.kind    = MachInstKind::MovReg;
					inst.encoding = EncodeMovReg(inst.rd, inst.rn, inst.sf);
					++changed;
				}
				break;
			case MachInstKind::SubReg:
				// SUB Xd, XZR, Xm leaves −Xm; too complex, skip
				// SUB Xd, Xn, XZR → MOV Xd, Xn
				if (inst.rm == kXZR) {
					inst.kind    = MachInstKind::MovReg;
					inst.encoding = EncodeMovReg(inst.rd, inst.rn, inst.sf);
					++changed;
				}
				break;
			default:
				break;
		}
	}
	return changed;
}

// ─── Pass 3: Constant Folding ─────────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassConstantFolding(std::vector<Arm64MachineInst>& insts) {
	uint32_t folded = 0;
	// Look for MOVZ Xd, #K  followed immediately by ADD Xd, Xd, #C
	for (size_t i = 0; i + 1 < insts.size(); ++i) {
		if (insts[i].is_dead || insts[i+1].is_dead) continue;
		if (insts[i].kind != MachInstKind::MovImm)  continue;
		if (insts[i+1].kind != MachInstKind::AddImm) continue;

		auto& movz = insts[i];
		auto& add  = insts[i+1];

		// ADD must be writing the same register that MOVZ produced
		if (add.rn != movz.rd || add.rd != movz.rd) continue;

		// Fold: result = MOVZ-value + imm
		int64_t result = movz.imm + add.imm;
		// Only valid for 16-bit zero-shifted result (hw=0)
		if (result >= 0 && result <= 0xFFFFu) {
			movz.imm      = result;
			movz.hw       = 0;
			movz.encoding = EncodeMovz(movz.rd, static_cast<uint16_t>(result), 0, movz.sf);
			add.kind      = MachInstKind::Nop;
			add.is_dead   = true;
			add.encoding  = kNopWord;
			++folded;
		}
	}
	return folded;
}

// ─── Pass 4: Immediate Folding ────────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassImmediateFolding(std::vector<Arm64MachineInst>& insts) {
	uint32_t folded = 0;
	for (auto& inst : insts) {
		if (inst.is_dead) continue;
		// ADD Xd, Xn, #0 where Xd == Xn → NOP
		if ((inst.kind == MachInstKind::AddImm || inst.kind == MachInstKind::SubImm)
		    && inst.imm == 0 && inst.rd == inst.rn) {
			inst.kind     = MachInstKind::Nop;
			inst.is_dead  = true;
			inst.encoding = kNopWord;
			++folded;
		}
		// MOV Xd, XZR → already XZR, still valid but mark immediate as 0
		else if (inst.kind == MachInstKind::AddImm && inst.imm == 0 && inst.rn == kXZR) {
			// ADD Xd, XZR, #0 → MOVZ Xd, #0
			inst.kind     = MachInstKind::MovImm;
			inst.imm      = 0;
			inst.hw       = 0;
			inst.encoding = EncodeMovz(inst.rd, 0, 0, inst.sf);
			++folded;
		}
	}
	return folded;
}

// ─── Pass 5: Instruction Combining ───────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassInstructionCombining(std::vector<Arm64MachineInst>& insts) {
	uint32_t combined = 0;
	// Pattern: LSL Xt, Xn, #k  ;  ADD Xd, Xm, Xt
	// → ADD Xd, Xm, Xn, LSL #k   (ADD shifted register with shift amount in imm6)
	for (size_t i = 0; i + 1 < insts.size(); ++i) {
		if (insts[i].is_dead || insts[i+1].is_dead) continue;
		if (insts[i].kind != MachInstKind::LslImm) continue;
		if (insts[i+1].kind != MachInstKind::AddReg) continue;

		auto& lsl = insts[i];
		auto& add = insts[i+1];

		// The shifted result register (lsl.rd) must feed into add as rm
		if (add.rm != lsl.rd) continue;
		// The shift amount must fit in imm6 (0-63)
		if (lsl.imm < 0 || lsl.imm > 63) continue;
		// Result register of LSL must not be used elsewhere after this window
		// (conservative: only combine if lsl.rd != add.rd and lsl.rd != add.rn)
		if (lsl.rd == add.rn || lsl.rd == add.rd) continue;

		// Encode ADD Xd, Xm(add.rn), Xn(lsl.rn), LSL #k
		// ADD (shifted register): sf | 000_01011 | shift(2) | 0 | Rm | imm6 | Rn | Rd
		uint32_t shift_type = 0u; // LSL
		uint32_t enc = 0x0B000000u;
		if (add.sf) enc |= (1u << 31u);
		enc |= (shift_type << 22u);
		enc |= ((lsl.rn & 0x1Fu) << 16u);  // Rm = the register being shifted
		enc |= ((lsl.imm & 0x3Fu) << 10u); // imm6 = shift amount
		enc |= ((add.rn & 0x1Fu) << 5u);   // Rn = other addend
		enc |= (add.rd & 0x1Fu);           // Rd = destination

		add.encoding = enc;
		add.rm       = lsl.rn;
		// Mark LSL dead
		lsl.kind     = MachInstKind::Nop;
		lsl.is_dead  = true;
		lsl.encoding = kNopWord;
		++combined;
	}
	return combined;
}

// ─── Pass 6: Register Coalescing ─────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassRegisterCoalescing(std::vector<Arm64MachineInst>& insts) {
	uint32_t coalesced = 0;
	// For each MOV Xd, Xs: if Xd is only ever used as an input and Xs is live,
	// rename all uses of Xd to Xs and remove the MOV.
	for (size_t i = 0; i < insts.size(); ++i) {
		if (insts[i].is_dead) continue;
		if (insts[i].kind != MachInstKind::MovReg) continue;

		uint8_t dst = insts[i].rd;
		uint8_t src = insts[i].rn;
		if (dst == src || dst == kXZR || src == kXZR) continue;

		// Check that dst is not written between i and end (conservative)
		// and that src is not overwritten between i and end
		bool dst_overwritten = false;
		bool src_overwritten = false;

		for (size_t j = i + 1; j < insts.size(); ++j) {
			const auto& jInst = insts[j];
			if (jInst.is_dead) continue;
			if (HasSideEffects(jInst)) { break; } // stop at any side-effecting inst to be safe
			if (jInst.rd == dst) { dst_overwritten = true; break; }
			if (jInst.rd == src) { src_overwritten = true; break; }
		}

		if (dst_overwritten || src_overwritten) continue;

		// Rename all subsequent uses of dst → src
		bool did_rename = false;
		for (size_t j = i + 1; j < insts.size(); ++j) {
			auto& jInst = insts[j];
			if (jInst.is_dead) continue;
			if (HasSideEffects(jInst)) break;
			if (jInst.rd == dst) break; // stop if dst is re-defined

			bool renamed = false;
			if (jInst.rn == dst) { jInst.rn = src; renamed = true; }
			if (jInst.rm == dst) { jInst.rm = src; renamed = true; }
			if (renamed) {
				// Re-encode the modified instruction
				jInst.encoding = Encode(jInst);
				did_rename = true;
			}
		}

		if (did_rename) {
			insts[i].kind     = MachInstKind::Nop;
			insts[i].is_dead  = true;
			insts[i].encoding = kNopWord;
			++coalesced;
		}
	}
	return coalesced;
}

// ─── Pass 7: Load/Store Folding ───────────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassLoadStoreFolding(std::vector<Arm64MachineInst>& insts) {
	uint32_t folded = 0;
	// Pattern: STR Xn, [Xb, #off]  ;  ... (no store to same slot)  ;  LDR Xm, [Xb, #off]
	// → LDR replaced by MOV Xm, Xn (forward store-to-load)
	for (size_t i = 0; i < insts.size(); ++i) {
		if (insts[i].is_dead) continue;
		if (insts[i].kind != MachInstKind::Store) continue;

		uint8_t str_rn  = insts[i].rn;  // base register
		int64_t str_off = insts[i].imm; // offset
		uint8_t str_rt  = insts[i].rd;  // value register

		// Search forward for matching LDR within a small window
		for (size_t j = i + 1; j < insts.size() && j <= i + 6; ++j) {
			if (insts[j].is_dead) continue;

			// If there's another store to the same address, stop
			if (insts[j].kind == MachInstKind::Store
			    && insts[j].rn == str_rn && insts[j].imm == str_off) break;

			// If the load address base is overwritten, stop
			if (insts[j].rd == str_rn) break;

			// Found a matching load
			if (insts[j].kind == MachInstKind::Load
			    && insts[j].rn == str_rn && insts[j].imm == str_off) {

				uint8_t ldr_rd = insts[j].rd;
				// Replace LDR with MOV Xm, Xn
				insts[j].kind     = MachInstKind::MovReg;
				insts[j].rn       = str_rt;
				insts[j].rd       = ldr_rd;
				insts[j].encoding = EncodeMovReg(ldr_rd, str_rt, true);
				++folded;
				break;
			}
		}
	}
	return folded;
}

// ─── Pass 8: Dead Code Elimination ───────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassDeadCodeElimination(std::vector<Arm64MachineInst>& insts) {
	uint32_t removed = 0;
	const size_t N = insts.size();

	// Compute live registers backwards from the end.
	// A register is live if it is read before being written.
	// Live-out set: assume all callee-saved + return regs live at exit.
	std::array<bool, 32> live = {};
	// AAPCS64 return registers (X0, X1) + FP(29), LR(30), SP(31) live at exit
	for (uint8_t r : {0,1,29,30,31}) live[r] = true;

	for (size_t i = N; i-- > 0; ) {
		auto& inst = insts[i];
		if (inst.is_dead) continue;
		if (HasSideEffects(inst)) {
			// Mark its inputs live
			if (inst.kind == MachInstKind::Store) live[inst.rd] = true;
			if (inst.rn < 32) live[inst.rn] = true;
			if (inst.rm < 32) live[inst.rm] = true;
			continue;
		}

		bool rd_live = (inst.rd < 32) && live[inst.rd];

		if (!rd_live && inst.rd != kXZR && inst.kind != MachInstKind::Nop) {
			// Instruction writes only to a dead register — safe to remove
			inst.is_dead  = true;
			inst.kind     = MachInstKind::Nop;
			inst.encoding = kNopWord;
			++removed;
		} else {
			// Mark sources live
			if (inst.rd < 32) live[inst.rd] = false; // register killed (defined)
			if (inst.rn < 32) live[inst.rn] = true;
			if (inst.rm < 32) live[inst.rm] = true;
		}
	}
	return removed;
}

// ─── Pass 9: Branch Simplification ───────────────────────────────────────────

uint32_t Arm64OptimizationPipeline::PassBranchSimplification(std::vector<Arm64MachineInst>& insts) {
	uint32_t simplified = 0;
	// Pattern: B.cond +8 (skip exactly one word which is a NOP)
	// → remove NOP instead; branch still terminates that path, but with 1 fewer inst
	for (size_t i = 0; i + 1 < insts.size(); ++i) {
		if (insts[i].is_dead) continue;
		auto& br = insts[i];
		if (br.kind != MachInstKind::BranchCond &&
		    br.kind != MachInstKind::CmpBranchZ  &&
		    br.kind != MachInstKind::CmpBranchNZ) continue;

		// Branch offset +1 word means it skips exactly the next instruction
		if (br.imm != 1) continue;

		// If the skipped instruction is a NOP, just remove the NOP
		auto& skip = insts[i + 1];
		if (skip.kind == MachInstKind::Nop || skip.is_dead) {
			skip.is_dead  = true;
			skip.encoding = kNopWord;
			++simplified;
		}
	}

	// Pattern: unconditional B offset=1 (jump over 1 NOP)
	for (size_t i = 0; i + 1 < insts.size(); ++i) {
		if (insts[i].is_dead) continue;
		auto& br = insts[i];
		if (br.kind != MachInstKind::Branch) continue;
		if (br.imm != 1) continue;
		auto& skip = insts[i + 1];
		if (skip.kind == MachInstKind::Nop || skip.is_dead) {
			// Branch is now a true no-op; remove both
			br.is_dead    = true;
			br.kind       = MachInstKind::Nop;
			br.encoding   = kNopWord;
			skip.is_dead  = true;
			skip.encoding = kNopWord;
			++simplified;
		}
	}
	return simplified;
}

// ─── Metrics ─────────────────────────────────────────────────────────────────

void Arm64OptimizationPipeline::ComputeMetrics(const std::vector<uint32_t>& code,
                                                Arm64OptStats& s, bool before) noexcept {
	uint32_t insts   = 0;
	uint32_t branches = 0;
	uint64_t cycles  = 0;

	for (uint32_t w : code) {
		auto inst = Decode(w);
		if (inst.kind == MachInstKind::Nop) continue;
		++insts;
		cycles += CycleFor(inst.kind);
		if (IsBranch(w)) ++branches;
	}

	uint32_t lines = (static_cast<uint32_t>(code.size() * 4) + 63u) / 64u;

	if (before) {
		s.insts_before             = insts;
		s.branches_before          = branches;
		s.code_bytes_before        = code.size() * 4;
		s.estimated_cycles_before  = cycles;
		s.cache_lines_before       = lines;
	} else {
		s.insts_after              = insts;
		s.branches_after           = branches;
		s.code_bytes_after         = code.size() * 4;
		s.estimated_cycles_after   = cycles;
		s.cache_lines_after        = lines;
	}
}

// ─── Pipeline Entry Point ─────────────────────────────────────────────────────

Arm64OptStats Arm64OptimizationPipeline::Run(std::vector<uint32_t>& code, const Arm64OptConfig& cfg) {
	Arm64OptStats stats;

	if (code.empty()) {
		m_last_stats = stats;
		return stats;
	}

	// Capture before-metrics
	ComputeMetrics(code, stats, /*before=*/true);

	// Decode all instructions into mutable representation
	std::vector<Arm64MachineInst> insts;
	insts.reserve(code.size());
	for (uint32_t w : code) insts.push_back(Decode(w));

	// ── Run Passes ───────────────────────────────────────────────────────────
	if (cfg.mov_elimination)
		stats.movs_eliminated     += PassMovElimination(insts);

	if (cfg.zero_register_opt)
		stats.zero_reg_opts       += PassZeroRegisterOpt(insts);

	if (cfg.constant_folding)
		stats.constants_folded    += PassConstantFolding(insts);

	if (cfg.immediate_folding)
		stats.immediates_folded   += PassImmediateFolding(insts);

	if (cfg.instruction_combining)
		stats.insts_combined      += PassInstructionCombining(insts);

	if (cfg.register_coalescing)
		stats.regs_coalesced      += PassRegisterCoalescing(insts);

	if (cfg.load_store_folding)
		stats.loads_stores_folded += PassLoadStoreFolding(insts);

	if (cfg.dead_code_elimination)
		stats.dead_insts_removed  += PassDeadCodeElimination(insts);

	if (cfg.branch_simplification)
		stats.branches_simplified += PassBranchSimplification(insts);

	// ── NOP Compaction: rebuild output vector ────────────────────────────────
	code.clear();
	for (const auto& inst : insts) {
		if (!inst.is_dead && inst.kind != MachInstKind::Nop) {
			code.push_back(inst.encoding);
		}
	}

	// Capture after-metrics
	ComputeMetrics(code, stats, /*before=*/false);

	m_last_stats = stats;
	return stats;
}

// ─── Disassembler ─────────────────────────────────────────────────────────────

std::string Arm64OptimizationPipeline::Disassemble(const std::vector<uint32_t>& code) {
	std::ostringstream ss;
	for (size_t i = 0; i < code.size(); ++i) {
		auto inst = Decode(code[i]);
		ss << "  [" << std::dec << i << "] 0x" << std::hex << code[i] << "  ";
		switch (inst.kind) {
			case MachInstKind::Nop:       ss << "NOP"; break;
			case MachInstKind::MovReg:    ss << "MOV X" << +inst.rd << ", X" << +inst.rn; break;
			case MachInstKind::MovImm:    ss << "MOVZ X" << +inst.rd << ", #" << std::dec << inst.imm; break;
			case MachInstKind::AddReg:    ss << "ADD X"  << +inst.rd << ", X" << +inst.rn << ", X" << +inst.rm; break;
			case MachInstKind::AddImm:    ss << "ADD X"  << +inst.rd << ", X" << +inst.rn << ", #" << std::dec << inst.imm; break;
			case MachInstKind::SubReg:    ss << "SUB X"  << +inst.rd << ", X" << +inst.rn << ", X" << +inst.rm; break;
			case MachInstKind::SubImm:    ss << "SUB X"  << +inst.rd << ", X" << +inst.rn << ", #" << std::dec << inst.imm; break;
			case MachInstKind::LslImm:    ss << "LSL X"  << +inst.rd << ", X" << +inst.rn << ", #" << std::dec << inst.imm; break;
			case MachInstKind::Load:      ss << "LDR X"  << +inst.rd << ", [X" << +inst.rn << ", #" << std::dec << inst.imm << "]"; break;
			case MachInstKind::Store:     ss << "STR X"  << +inst.rd << ", [X" << +inst.rn << ", #" << std::dec << inst.imm << "]"; break;
			case MachInstKind::LoadPair:  ss << "LDP X"  << +inst.rd << ", X" << +inst.rm << ", [X" << +inst.rn << ", #" << std::dec << inst.imm << "]"; break;
			case MachInstKind::StorePair: ss << "STP X"  << +inst.rd << ", X" << +inst.rm << ", [X" << +inst.rn << ", #" << std::dec << inst.imm << "]"; break;
			case MachInstKind::Branch:    ss << "B #"    << std::dec << inst.imm; break;
			case MachInstKind::BranchCond:ss << "B.cond(#" << std::dec << inst.cond_code << ") #" << inst.imm; break;
			case MachInstKind::CmpBranchZ: ss << "CBZ X" << +inst.rd << ", #" << std::dec << inst.imm; break;
			case MachInstKind::CmpBranchNZ:ss << "CBNZ X"<< +inst.rd << ", #" << std::dec << inst.imm; break;
			case MachInstKind::Ret:       ss << "RET"; break;
			default:                      ss << "?? (0x" << std::hex << code[i] << ")"; break;
		}
		ss << "\n";
	}
	return ss.str();
}

// ─── Stats Formatter ─────────────────────────────────────────────────────────

std::string Arm64OptimizationPipeline::FormatStats(const Arm64OptStats& s) {
	char buf[2048];
	std::snprintf(buf, sizeof(buf),
		"\n  %-28s  %8s  %8s  %8s\n"
		"  %s\n"
		"  %-28s  %8u  %8u  %+8d\n"
		"  %-28s  %8llu  %8llu  %+8lld\n"
		"  %-28s  %8zu  %8zu  %+8d\n"
		"  %-28s  %8u  %8u  %+8d\n"
		"  %-28s  %8u  %8u  %+8d\n"
		"  %s\n"
		"  MOV eliminated:         %u\n"
		"  Zero-reg opts:          %u\n"
		"  Constants folded:       %u\n"
		"  Immediates folded:      %u\n"
		"  Instructions combined:  %u\n"
		"  Registers coalesced:    %u\n"
		"  Load/stores folded:     %u\n"
		"  Dead insts removed:     %u\n"
		"  Branches simplified:    %u\n",
		"Metric", "Before", "After", "Delta",
		std::string(56, '-').c_str(),
		"Instructions",     s.insts_before, s.insts_after,           s.InstructionDelta(),
		"Estimated cycles", (unsigned long long)s.estimated_cycles_before,
		                    (unsigned long long)s.estimated_cycles_after, (long long)s.CycleDelta(),
		"Code bytes",       s.code_bytes_before, s.code_bytes_after,  s.ByteDelta(),
		"Branch count",     s.branches_before, s.branches_after,      s.BranchDelta(),
		"Cache lines",      s.cache_lines_before, s.cache_lines_after, s.CacheLineDelta(),
		std::string(56, '-').c_str(),
		s.movs_eliminated, s.zero_reg_opts, s.constants_folded, s.immediates_folded,
		s.insts_combined, s.regs_coalesced, s.loads_stores_folded,
		s.dead_insts_removed, s.branches_simplified
	);
	return buf;
}

} // namespace Loader::Recompiler
