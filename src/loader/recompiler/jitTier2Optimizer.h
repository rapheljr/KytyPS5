// jitTier2Optimizer.h
//
// Tier-2 Profile-Guided JIT Optimizer for KytyPS5 ARM64 Recompiler.
// Features Global Value Numbering (GVN), Loop Invariant Code Motion (LICM),
// and algebraic constant simplification on Control Flow Graphs (CFG).

#ifndef LOADER_RECOMPILER_JIT_TIER2_OPTIMIZER_H
#define LOADER_RECOMPILER_JIT_TIER2_OPTIMIZER_H

#include "common/common.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Loader::Recompiler {

struct Tier2OptimizationStats {
	uint32_t expressions_folded         = 0;
	uint32_t redundant_loads_eliminated = 0;
	uint32_t loop_invariants_hoisted    = 0;
	uint32_t dead_instructions_purged   = 0;
};

class JitTier2Optimizer {
public:
	JitTier2Optimizer() = default;
	~JitTier2Optimizer() = default;

	KYTY_CLASS_NO_COPY(JitTier2Optimizer);

	/// Run Tier-2 optimization passes on Control Flow Graph
	bool Optimize(ControlFlowGraph& cfg);

	/// Perform Constant Folding & Algebraic Simplification
	uint32_t RunConstantFolding(ControlFlowGraph& cfg);

	/// Perform Global Value Numbering (GVN)
	uint32_t RunGlobalValueNumbering(ControlFlowGraph& cfg);

	/// Perform Dead Code Elimination (DCE)
	uint32_t RunDeadCodeElimination(ControlFlowGraph& cfg);

	[[nodiscard]] const Tier2OptimizationStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = {}; }

private:
	Tier2OptimizationStats m_stats{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_TIER2_OPTIMIZER_H
