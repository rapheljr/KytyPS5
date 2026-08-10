// jitEscapeAnalysis.h
//
// JIT Tier-2 Escape Analysis & Stack Scalar Replacement (SROA) for KytyPS5.
// Identifies non-escaping guest stack locations and promotes them to virtual registers.

#ifndef LOADER_RECOMPILER_JIT_ESCAPE_ANALYSIS_H
#define LOADER_RECOMPILER_JIT_ESCAPE_ANALYSIS_H

#include "common/common.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

struct EscapeAnalysisStats {
	uint32_t stack_slots_tracked       = 0;
	uint32_t stack_stores_eliminated   = 0;
	uint32_t stack_loads_forwarded     = 0;
};

class JitEscapeAnalysis {
public:
	JitEscapeAnalysis();
	~JitEscapeAnalysis() = default;

	KYTY_CLASS_NO_COPY(JitEscapeAnalysis);

	/// Run escape analysis and scalar replacement pass on CFG
	bool RunPass(ControlFlowGraph& cfg);

	[[nodiscard]] const EscapeAnalysisStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	EscapeAnalysisStats m_stats{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_ESCAPE_ANALYSIS_H
