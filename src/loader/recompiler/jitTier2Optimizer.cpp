#include "loader/recompiler/jitTier2Optimizer.h"
#include "loader/recompiler/jitEscapeAnalysis.h"
#include "loader/recompiler/jitFunctionInliner.h"
#include "loader/recompiler/jitLoopVectorizer.h"
#include "common/profiler.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Loader::Recompiler {

bool JitTier2Optimizer::Optimize(ControlFlowGraph& cfg) {
	KYTY_PROFILER_FUNCTION();

	// 1. Run Function Inlining Pass
	JitFunctionInliner inliner;
	inliner.RunPass(cfg);

	// 2. Run Escape Analysis & Stack Scalar Replacement
	JitEscapeAnalysis escape_analysis;
	escape_analysis.RunPass(cfg);

	// 3. Run Auto-SIMD Loop Vectorization Pass
	JitLoopVectorizer loop_vectorizer;
	loop_vectorizer.RunPass(cfg);

	uint32_t passes = 0;
	bool changed = true;

	while (changed && passes < 4) {
		changed = false;
		uint32_t cf  = RunConstantFolding(cfg);
		uint32_t gvn = RunGlobalValueNumbering(cfg);
		uint32_t dce = RunDeadCodeElimination(cfg);

		if (cf > 0 || gvn > 0 || dce > 0) {
			changed = true;
		}
		passes++;
	}

	return true;
}

uint32_t JitTier2Optimizer::RunConstantFolding(ControlFlowGraph& cfg) {
	uint32_t count = 0;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			auto& operands = inst->GetOperands();
			// 1. ADD x, 0 -> MOV x; ADD c1, c2 -> (c1 + c2)
			if (inst->GetOpcode() == IROpcode::Add && operands.size() >= 2) {
				if (operands[1].IsImmInt() && operands[1].imm_int == 0) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t sum = operands[0].imm_int + operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(sum));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 2. SUB x, 0 -> MOV x; SUB x, x -> 0; SUB c1, c2 -> (c1 - c2)
			else if (inst->GetOpcode() == IROpcode::Sub && operands.size() >= 2) {
				if (operands[1].IsImmInt() && operands[1].imm_int == 0) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsVReg() && operands[1].IsVReg() && operands[0].vreg == operands[1].vreg) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(0));
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t diff = operands[0].imm_int - operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(diff));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 3. MUL x, 0 -> 0; MUL x, 1 -> x; MUL c1, c2 -> (c1 * c2)
			else if (inst->GetOpcode() == IROpcode::Mul && operands.size() >= 2) {
				if ((operands[1].IsImmInt() && operands[1].imm_int == 0) ||
				    (operands[0].IsImmInt() && operands[0].imm_int == 0)) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(0));
					count++;
					m_stats.expressions_folded++;
				} else if (operands[1].IsImmInt() && operands[1].imm_int == 1) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t prod = operands[0].imm_int * operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(prod));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 4. AND x, 0 -> 0; AND x, x -> x; AND c1, c2 -> (c1 & c2)
			else if (inst->GetOpcode() == IROpcode::And && operands.size() >= 2) {
				if (operands[1].IsImmInt() && operands[1].imm_int == 0) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(0));
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsVReg() && operands[1].IsVReg() && operands[0].vreg == operands[1].vreg) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t res = operands[0].imm_int & operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(res));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 5. OR x, 0 -> x; OR x, x -> x; OR c1, c2 -> (c1 | c2)
			else if (inst->GetOpcode() == IROpcode::Or && operands.size() >= 2) {
				if (operands[1].IsImmInt() && operands[1].imm_int == 0) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsVReg() && operands[1].IsVReg() && operands[0].vreg == operands[1].vreg) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t res = operands[0].imm_int | operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(res));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 6. XOR x, x -> 0; XOR c1, c2 -> (c1 ^ c2)
			else if (inst->GetOpcode() == IROpcode::Xor && operands.size() >= 2) {
				if (operands[0].IsVReg() && operands[1].IsVReg() && operands[0].vreg == operands[1].vreg) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(0));
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t res = operands[0].imm_int ^ operands[1].imm_int;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(res));
					count++;
					m_stats.expressions_folded++;
				}
			}
			// 7. Shifts & Bitwise Counts
			else if ((inst->GetOpcode() == IROpcode::Shl || inst->GetOpcode() == IROpcode::LShr || inst->GetOpcode() == IROpcode::AShr ||
			          inst->GetOpcode() == IROpcode::Shlx || inst->GetOpcode() == IROpcode::Shrx || inst->GetOpcode() == IROpcode::Sarx) && operands.size() >= 2) {
				if (operands[1].IsImmInt() && operands[1].imm_int == 0) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.resize(1);
					count++;
					m_stats.expressions_folded++;
				} else if (operands[0].IsImmInt() && operands[1].IsImmInt()) {
					int64_t shift = operands[1].imm_int & 63;
					int64_t res = 0;
					if (inst->GetOpcode() == IROpcode::Shl || inst->GetOpcode() == IROpcode::Shlx) res = operands[0].imm_int << shift;
					else if (inst->GetOpcode() == IROpcode::LShr || inst->GetOpcode() == IROpcode::Shrx) res = static_cast<int64_t>(static_cast<uint64_t>(operands[0].imm_int) >> shift);
					else res = operands[0].imm_int >> shift;
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(res));
					count++;
					m_stats.expressions_folded++;
				}
			}
			else if (inst->GetOpcode() == IROpcode::Popcnt && !operands.empty() && operands[0].IsImmInt()) {
				int64_t count_val = __builtin_popcountll(static_cast<uint64_t>(operands[0].imm_int));
				inst->SetOpcode(IROpcode::ZExt);
				operands.clear();
				operands.push_back(Value::MakeImmInt(count_val));
				count++;
				m_stats.expressions_folded++;
			}
		}
	}

	return count;
}

uint32_t JitTier2Optimizer::RunGlobalValueNumbering(ControlFlowGraph& cfg) {
	uint32_t count = 0;
	std::unordered_map<std::string, VirtualReg> expression_map;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || !inst->HasDst()) continue;

			IROpcode op = inst->GetOpcode();
			if (op == IROpcode::Add || op == IROpcode::Sub || op == IROpcode::Mul ||
			    op == IROpcode::And || op == IROpcode::Or  || op == IROpcode::Xor ||
			    op == IROpcode::Shl || op == IROpcode::LShr || op == IROpcode::AShr ||
			    op == IROpcode::Shlx || op == IROpcode::Shrx || op == IROpcode::Sarx) {
				const auto& operands = inst->GetOperands();
				if (operands.size() < 2) continue;

				std::stringstream ss;
				ss << static_cast<int>(op) << "_"
				   << (operands[0].IsImmInt() ? operands[0].imm_int : operands[0].vreg.id) << "_"
				   << (operands[1].IsImmInt() ? operands[1].imm_int : operands[1].vreg.id);
				std::string key = ss.str();

				auto it = expression_map.find(key);
				if (it != expression_map.end()) {
					// Redundant calculation found -> convert to ZExt from existing virtual register
					inst->SetOpcode(IROpcode::ZExt);
					inst->GetOperands().clear();
					inst->AddOperand(Value::MakeVReg(it->second));
					count++;
					m_stats.redundant_loads_eliminated++;
				} else {
					expression_map[key] = inst->GetDst();
				}
			}
		}
	}

	return count;
}

uint32_t JitTier2Optimizer::RunDeadCodeElimination(ControlFlowGraph& cfg) {
	uint32_t count = 0;
	std::unordered_set<uint32_t> used_vregs;

	// Collect all referenced virtual registers
	for (const auto& block : cfg.GetBlocks()) {
		for (const auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;
			for (const auto& op : inst->GetOperands()) {
				if (op.IsVReg()) {
					used_vregs.insert(op.vreg.id);
				}
			}
		}
	}

	// Deactivate instructions producing unused outputs
	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || inst->IsTerminator() || inst->GetOpcode() == IROpcode::Store) {
				continue;
			}
			if (inst->HasDst() && used_vregs.count(inst->GetDst().id) == 0) {
				inst->SetActive(false);
				count++;
				m_stats.dead_instructions_purged++;
			}
		}
	}

	return count;
}

} // namespace Loader::Recompiler
