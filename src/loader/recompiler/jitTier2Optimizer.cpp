// jitTier2Optimizer.cpp
//
// Tier-2 Profile-Guided JIT Optimizer Implementation.

#include "loader/recompiler/jitTier2Optimizer.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace Loader::Recompiler {

bool JitTier2Optimizer::Optimize(ControlFlowGraph& cfg) {
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
			// 1. ADD x, 0 -> MOV x (or identity)
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
			// 2. XOR x, x -> 0
			else if (inst->GetOpcode() == IROpcode::Xor && operands.size() >= 2) {
				if (operands[0].IsVReg() && operands[1].IsVReg() && operands[0].vreg == operands[1].vreg) {
					inst->SetOpcode(IROpcode::ZExt);
					operands.clear();
					operands.push_back(Value::MakeImmInt(0));
					count++;
					m_stats.expressions_folded++;
				}
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

			if (inst->GetOpcode() == IROpcode::Add || inst->GetOpcode() == IROpcode::Sub || inst->GetOpcode() == IROpcode::Mul) {
				const auto& operands = inst->GetOperands();
				if (operands.size() < 2) continue;

				std::stringstream ss;
				ss << static_cast<int>(inst->GetOpcode()) << "_"
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
