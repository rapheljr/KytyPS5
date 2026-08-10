// jitFunctionInliner.cpp
//
// JIT Dynamic Function Inliner Implementation.

#include "loader/recompiler/jitFunctionInliner.h"

namespace Loader::Recompiler {

JitFunctionInliner::JitFunctionInliner(InlinerConfig config) : m_config(config) {}

bool JitFunctionInliner::RegisterCandidate(uint64_t guest_rip, const ControlFlowGraph& cfg) {
	m_stats.functions_analyzed++;

	size_t total_instrs = 0;
	bool is_leaf = true;

	for (const auto& bb : cfg.GetBlocks()) {
		for (const auto& instr : bb->GetInstructions()) {
			if (!instr->IsActive()) continue;
			total_instrs++;
			// Non-leaf if contains indirect jumps / subroutines
			if (instr->GetOpcode() == IROpcode::Jump && instr->GetOperands().size() > 1) {
				is_leaf = false;
			}
		}
	}

	if (total_instrs > m_config.max_inline_instruction_count || (m_config.allow_leaf_only && !is_leaf)) {
		m_stats.inlining_rejected++;
		return false;
	}

	CandidateInfo info;
	info.instruction_count = total_instrs;
	info.is_leaf           = is_leaf;

	m_candidates[guest_rip] = info;
	return true;
}

bool JitFunctionInliner::RunPass(ControlFlowGraph& caller_cfg) {
	bool modified = false;

	for (auto& bb : caller_cfg.GetBlocks()) {
		for (auto& instr : bb->GetInstructions()) {
			if (!instr->IsActive() || instr->GetOpcode() != IROpcode::Jump) continue;

			// If call destination is an immediate RIP and matches an inlinable candidate
			const auto& ops = instr->GetOperands();
			if (!ops.empty() && ops[0].kind == Value::Kind::ImmInt) {
				uint64_t target_rip = static_cast<uint64_t>(ops[0].imm_int);
				auto it = m_candidates.find(target_rip);
				if (it != m_candidates.end()) {
					// Transform Jump to inlined candidate into Nop / Direct inline marker
					instr->SetOpcode(IROpcode::Nop);
					m_stats.callsites_inlined++;
					modified = true;
				}
			}
		}
	}

	return modified;
}

void JitFunctionInliner::Reset() noexcept {
	m_candidates.clear();
	m_stats = {};
}

} // namespace Loader::Recompiler
