// jitFunctionInliner.h
//
// JIT Dynamic Function Inliner for KytyPS5 Recompiler.
// Analyzes guest small leaf functions and inlines their IR instructions directly at call sites.

#ifndef LOADER_RECOMPILER_JIT_FUNCTION_INLINER_H
#define LOADER_RECOMPILER_JIT_FUNCTION_INLINER_H

#include "common/common.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

struct InlinerConfig {
	size_t max_inline_instruction_count = 16;
	bool   allow_leaf_only              = true;
};

struct InlinerStats {
	uint32_t functions_analyzed = 0;
	uint32_t callsites_inlined  = 0;
	uint32_t inlining_rejected  = 0;
};

class JitFunctionInliner {
public:
	explicit JitFunctionInliner(InlinerConfig config = {});
	~JitFunctionInliner() = default;

	KYTY_CLASS_NO_COPY(JitFunctionInliner);

	/// Register a guest function candidate for inlining
	bool RegisterCandidate(uint64_t guest_rip, const ControlFlowGraph& cfg);

	/// Attempt to inline registered functions into caller CFG
	bool RunPass(ControlFlowGraph& caller_cfg);

	[[nodiscard]] bool IsCandidate(uint64_t guest_rip) const noexcept {
		return m_candidates.find(guest_rip) != m_candidates.end();
	}

	[[nodiscard]] const InlinerStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	struct CandidateInfo {
		uint64_t instruction_count = 0;
		bool     is_leaf           = true;
	};

	InlinerConfig                               m_config;
	std::unordered_map<uint64_t, CandidateInfo> m_candidates;
	InlinerStats                                m_stats{};
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_FUNCTION_INLINER_H
