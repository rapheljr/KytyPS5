// arm64OptimizationPipeline.h
//
// ARM64 Post-Lowering Machine-Level Peephole Optimization Pipeline.
//
// Operates on the raw uint32_t instruction stream produced by Arm64Emitter.
// Never changes observable behavior. Every optimized block is differentially
// validated; any block failing validation is silently rolled back.
//
// Pipeline order (fixed, single-pass):
//   1. MOV Elimination
//   2. Zero Register Optimization
//   3. Constant Folding
//   4. Immediate Folding
//   5. Instruction Combining
//   6. Register Coalescing
//   7. Load / Store Folding
//   8. Dead Code Elimination
//   9. Branch Simplification
//  10. NOP Compaction (final sweep)

#ifndef LOADER_RECOMPILER_ARM64_OPTIMIZATION_PIPELINE_H
#define LOADER_RECOMPILER_ARM64_OPTIMIZATION_PIPELINE_H

#include "common/common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Loader::Recompiler {

// ─── Machine Instruction Kind ────────────────────────────────────────────────

enum class MachInstKind : uint8_t {
	Nop,           // NOP / placeholder
	MovReg,        // MOV Xd, Xn  (ORR Xd, XZR, Xn)
	MovImm,        // MOVZ / MOVK / MOVN
	AddReg,        // ADD Xd, Xn, Xm
	AddImm,        // ADD Xd, Xn, #imm
	SubReg,        // SUB Xd, Xn, Xm
	SubImm,        // SUB Xd, Xn, #imm
	AndReg,        // AND Xd, Xn, Xm
	OrrReg,        // ORR Xd, Xn, Xm
	EorReg,        // EOR Xd, Xn, Xm
	LslImm,        // LSL Xd, Xn, #shift  (UBFM alias)
	LsrImm,        // LSR Xd, Xn, #shift
	AsrImm,        // ASR Xd, Xn, #shift
	Mul,           // MUL Xd, Xn, Xm
	Load,          // LDR Xd, [Xn, #off]
	Store,         // STR Xd, [Xn, #off]
	LoadPair,      // LDP Xd1, Xd2, [Xn, #off]
	StorePair,     // STP Xs1, Xs2, [Xn, #off]
	Branch,        // B #off
	BranchLink,    // BL #off
	BranchReg,     // BR Xn
	BranchCond,    // B.cond #off
	CmpBranchZ,    // CBZ  Xn, #off
	CmpBranchNZ,   // CBNZ Xn, #off
	Ret,           // RET
	Other,         // Anything else — passed through untouched
};

// ─── Decoded Machine Instruction ─────────────────────────────────────────────

struct Arm64MachineInst {
	uint32_t    encoding  = 0;             // original 32-bit word
	MachInstKind kind     = MachInstKind::Other;
	uint8_t     rd        = 31;            // destination (31 = XZR / SP)
	uint8_t     rn        = 31;            // first source
	uint8_t     rm        = 31;            // second source
	int64_t     imm       = 0;             // immediate / offset / shift amount
	uint8_t     hw        = 0;             // MOVZ/MOVK hw field (shift16)
	bool        sf        = true;          // 1 = 64-bit, 0 = 32-bit
	bool        is_dead   = false;         // marked for removal by DCE
	bool        sets_flags = false;        // writes NZCV
	uint32_t    cond_code = 0;             // for B.cond (ARM64 condition encoding)
};

// ─── Optimization Config ─────────────────────────────────────────────────────

struct Arm64OptConfig {
	bool mov_elimination        = true;   // MOV Xd,Xd; MOV XZR,... identity
	bool zero_register_opt      = true;   // AND/ORR with XZR → MOV XZR
	bool constant_folding       = true;   // MOVZ;ADD #imm → MOVZ(result)
	bool immediate_folding      = true;   // ADD/SUB #0 → NOP
	bool instruction_combining  = true;   // LSL+ADD → ADD-shifted
	bool register_coalescing    = true;   // MOV copy-chain elimination
	bool load_store_folding     = true;   // STR+LDR same slot → MOV forwarding
	bool dead_code_elimination  = true;   // writes to dead registers
	bool branch_simplification  = true;   // branch-over-nop → delete nop
};

// ─── Optimization Statistics ─────────────────────────────────────────────────

struct Arm64OptStats {
	// Instruction counts
	uint32_t insts_before            = 0;
	uint32_t insts_after             = 0;

	// Per-pass removal counts
	uint32_t movs_eliminated         = 0;
	uint32_t zero_reg_opts           = 0;
	uint32_t constants_folded        = 0;
	uint32_t immediates_folded       = 0;
	uint32_t insts_combined          = 0;
	uint32_t regs_coalesced          = 0;
	uint32_t loads_stores_folded     = 0;
	uint32_t dead_insts_removed      = 0;
	uint32_t branches_simplified     = 0;

	// Branch counts
	uint32_t branches_before         = 0;
	uint32_t branches_after          = 0;

	// Size metrics
	size_t   code_bytes_before       = 0;
	size_t   code_bytes_after        = 0;

	// Cycle estimates (1 cycle / non-memory inst, 4 cycles / load/store)
	uint64_t estimated_cycles_before = 0;
	uint64_t estimated_cycles_after  = 0;

	// Cache-line footprint (64 bytes per cache line)
	uint32_t cache_lines_before      = 0;
	uint32_t cache_lines_after       = 0;

	[[nodiscard]] int32_t InstructionDelta()  const noexcept { return int32_t(insts_after)  - int32_t(insts_before); }
	[[nodiscard]] int64_t CycleDelta()        const noexcept { return int64_t(estimated_cycles_after) - int64_t(estimated_cycles_before); }
	[[nodiscard]] int32_t ByteDelta()         const noexcept { return int32_t(code_bytes_after) - int32_t(code_bytes_before); }
	[[nodiscard]] int32_t BranchDelta()       const noexcept { return int32_t(branches_after) - int32_t(branches_before); }
	[[nodiscard]] int32_t CacheLineDelta()    const noexcept { return int32_t(cache_lines_after) - int32_t(cache_lines_before); }
};

// ─── Pipeline ────────────────────────────────────────────────────────────────

class Arm64OptimizationPipeline {
public:
	Arm64OptimizationPipeline() = default;
	~Arm64OptimizationPipeline() = default;

	KYTY_CLASS_NO_COPY(Arm64OptimizationPipeline);

	// Run all enabled passes on `code` (in-place).
	// Returns statistics comparing before and after.
	[[nodiscard]] Arm64OptStats Run(std::vector<uint32_t>& code, const Arm64OptConfig& cfg = {});

	[[nodiscard]] const Arm64OptStats& GetLastStats() const noexcept { return m_last_stats; }

	// Decode a single ARM64 instruction word to Arm64MachineInst.
	[[nodiscard]] static Arm64MachineInst Decode(uint32_t word) noexcept;

	// Encode an Arm64MachineInst back to a uint32_t. Returns the original
	// encoding for kinds that were not modified (Other, etc.).
	[[nodiscard]] static uint32_t Encode(const Arm64MachineInst& inst) noexcept;

	// Pretty-print a decoded stream for debugging.
	[[nodiscard]] static std::string Disassemble(const std::vector<uint32_t>& code);

	// Pretty-print the before/after stats table.
	[[nodiscard]] static std::string FormatStats(const Arm64OptStats& s);

private:
	// ── Pass Implementations ─────────────────────────────────────────────────
	static uint32_t PassMovElimination      (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassZeroRegisterOpt     (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassConstantFolding     (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassImmediateFolding    (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassInstructionCombining(std::vector<Arm64MachineInst>& insts);
	static uint32_t PassRegisterCoalescing  (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassLoadStoreFolding    (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassDeadCodeElimination (std::vector<Arm64MachineInst>& insts);
	static uint32_t PassBranchSimplification(std::vector<Arm64MachineInst>& insts);

	// ── Decode Helpers ───────────────────────────────────────────────────────
	[[nodiscard]] static bool IsMovReg      (uint32_t w, uint8_t& rd, uint8_t& rn) noexcept;
	[[nodiscard]] static bool IsMovz        (uint32_t w, uint8_t& rd, uint16_t& imm16, uint8_t& hw) noexcept;
	[[nodiscard]] static bool IsNop         (uint32_t w) noexcept;
	[[nodiscard]] static bool IsLoad        (uint32_t w) noexcept;
	[[nodiscard]] static bool IsStore       (uint32_t w) noexcept;
	[[nodiscard]] static bool IsBranch      (uint32_t w) noexcept;
	[[nodiscard]] static bool HasSideEffects(const Arm64MachineInst& i) noexcept;

	// ── Encode Helpers ───────────────────────────────────────────────────────
	[[nodiscard]] static uint32_t EncodeMovReg(uint8_t rd, uint8_t rn, bool sf) noexcept;
	[[nodiscard]] static uint32_t EncodeMovz  (uint8_t rd, uint16_t imm16, uint8_t hw, bool sf) noexcept;
	[[nodiscard]] static uint32_t EncodeAddImm(uint8_t rd, uint8_t rn, uint32_t imm12, bool sf) noexcept;
	[[nodiscard]] static uint32_t EncodeNop   () noexcept;

	// ── Metrics ──────────────────────────────────────────────────────────────
	static void ComputeMetrics(const std::vector<uint32_t>& code, Arm64OptStats& s, bool before) noexcept;

	Arm64OptStats m_last_stats;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_OPTIMIZATION_PIPELINE_H
