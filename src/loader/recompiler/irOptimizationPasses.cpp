// irOptimizationPasses.cpp
//
// Target-Independent Optimization Passes & PassManager for Target-Independent IR.

#include "loader/recompiler/irOptimizationPasses.h"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace Loader::Recompiler {

// 1. Constant Propagation Pass
bool ConstantPropagationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;
	std::unordered_map<uint32_t, int64_t> const_map;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			// Replace VReg operands with known constants
			auto& ops = inst->GetOperands();
			for (size_t i = 0; i < ops.size(); ++i) {
				if (ops[i].IsVReg() && const_map.count(ops[i].vreg.id)) {
					ops[i] = Value::MakeImmInt(const_map[ops[i].vreg.id], ops[i].vreg.type);
					modified = true;
				}
			}

			// Track definitions of constant values
			if (inst->HasDst()) {
				if (inst->GetOpcode() == IROpcode::Nop) {
					// no constant
				} else if (ops.size() == 1 && ops[0].IsImmInt()) {
					const_map[inst->GetDst().id] = ops[0].imm_int;
				}
			}
		}
	}
	return modified;
}

// 2. Constant Folding Pass
bool ConstantFoldingPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || !inst->HasDst()) continue;

			const auto& ops = inst->GetOperands();
			if (ops.size() == 2 && ops[0].IsImmInt() && ops[1].IsImmInt()) {
				int64_t val1 = ops[0].imm_int;
				int64_t val2 = ops[1].imm_int;
				int64_t result = 0;
				bool can_fold = true;

				switch (inst->GetOpcode()) {
					case IROpcode::Add:  result = val1 + val2; break;
					case IROpcode::Sub:  result = val1 - val2; break;
					case IROpcode::Mul:  result = val1 * val2; break;
					case IROpcode::And:  result = val1 & val2; break;
					case IROpcode::Or:   result = val1 | val2; break;
					case IROpcode::Xor:  result = val1 ^ val2; break;
					case IROpcode::Shl:  result = val1 << (val2 & 63); break;
					case IROpcode::LShr: result = static_cast<uint64_t>(val1) >> (val2 & 63); break;
					case IROpcode::SDiv:
						if (val2 != 0) result = val1 / val2; else can_fold = false;
						break;
					case IROpcode::UDiv:
						if (val2 != 0) result = static_cast<uint64_t>(val1) / static_cast<uint64_t>(val2); else can_fold = false;
						break;
					default:
						can_fold = false;
						break;
				}

				if (can_fold) {
					inst->SetOpcode(IROpcode::Nop);
					inst->GetOperands().clear();
					inst->AddOperand(Value::MakeImmInt(result, inst->GetDst().type));
					modified = true;
				}
			}
		}
	}
	return modified;
}

// 3. Copy Propagation Pass
bool CopyPropagationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;
	std::unordered_map<uint32_t, VirtualReg> copy_map;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			auto& ops = inst->GetOperands();
			for (size_t i = 0; i < ops.size(); ++i) {
				if (ops[i].IsVReg() && copy_map.count(ops[i].vreg.id)) {
					ops[i].vreg = copy_map[ops[i].vreg.id];
					modified = true;
				}
			}

			// Record copies: dst = src_vreg (e.g. Add dst, src, 0 or direct Move)
			if (inst->HasDst() && ops.size() == 1 && ops[0].IsVReg()) {
				copy_map[inst->GetDst().id] = ops[0].vreg;
			}
		}
	}
	return modified;
}

// 4. Algebraic Simplification Pass
bool AlgebraicSimplificationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || !inst->HasDst()) continue;

			const auto& ops = inst->GetOperands();
			if (ops.size() == 2) {
				// x + 0 -> x, x * 1 -> x, x - 0 -> x, x | 0 -> x, x ^ 0 -> x
				if (ops[1].IsImmInt() && ops[1].imm_int == 0) {
					if (inst->GetOpcode() == IROpcode::Add || inst->GetOpcode() == IROpcode::Sub ||
					    inst->GetOpcode() == IROpcode::Or  || inst->GetOpcode() == IROpcode::Xor ||
					    inst->GetOpcode() == IROpcode::Shl || inst->GetOpcode() == IROpcode::LShr) {
						inst->SetOpcode(IROpcode::Nop);
						VirtualReg dst = inst->GetDst();
						inst->GetOperands().clear();
						inst->AddOperand(ops[0]);
						inst->SetDst(dst);
						modified = true;
					} else if (inst->GetOpcode() == IROpcode::Mul || inst->GetOpcode() == IROpcode::And) {
						// x * 0 -> 0, x & 0 -> 0
						inst->SetOpcode(IROpcode::Nop);
						inst->GetOperands().clear();
						inst->AddOperand(Value::MakeImmInt(0, inst->GetDst().type));
						modified = true;
					}
				} else if (ops[1].IsImmInt() && ops[1].imm_int == 1 && inst->GetOpcode() == IROpcode::Mul) {
					// x * 1 -> x
					inst->SetOpcode(IROpcode::Nop);
					inst->GetOperands().clear();
					inst->AddOperand(ops[0]);
					modified = true;
				} else if (ops[0].IsVReg() && ops[1].IsVReg() && ops[0].vreg == ops[1].vreg) {
					// x - x -> 0, x ^ x -> 0
					if (inst->GetOpcode() == IROpcode::Sub || inst->GetOpcode() == IROpcode::Xor) {
						inst->SetOpcode(IROpcode::Nop);
						inst->GetOperands().clear();
						inst->AddOperand(Value::MakeImmInt(0, inst->GetDst().type));
						modified = true;
					}
				}
			}
		}
	}
	return modified;
}

// 5. Common Subexpression Elimination Pass (CSE)
bool CommonSubexpressionEliminationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		std::unordered_map<std::string, VirtualReg> expr_map;

		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || !inst->HasDst() || inst->GetOpcode() == IROpcode::Nop) continue;

			const auto& ops = inst->GetOperands();
			if (ops.size() == 2) {
				std::string key = std::to_string(static_cast<int>(inst->GetOpcode())) + "_" +
				                  (ops[0].IsVReg() ? ("v" + std::to_string(ops[0].vreg.id)) : ("i" + std::to_string(ops[0].imm_int))) + "_" +
				                  (ops[1].IsVReg() ? ("v" + std::to_string(ops[1].vreg.id)) : ("i" + std::to_string(ops[1].imm_int)));

				if (expr_map.count(key)) {
					// Replace redundant instruction with copy from previous expression vreg
					inst->SetOpcode(IROpcode::Nop);
					inst->GetOperands().clear();
					inst->AddOperand(Value::MakeVReg(expr_map[key]));
					modified = true;
				} else {
					expr_map[key] = inst->GetDst();
				}
			}
		}
	}
	return modified;
}

// 6. Branch Simplification Pass
bool BranchSimplificationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			if (inst->GetOpcode() == IROpcode::BranchCond) {
				const auto& ops = inst->GetOperands();
				if (ops.size() >= 3 && ops[0].IsImmInt()) {
					int64_t cond_val = ops[0].imm_int;
					BasicBlock* target_true  = ops[1].block_ref;
					BasicBlock* target_false = ops[2].block_ref;

					BasicBlock* taken = (cond_val != 0) ? target_true : target_false;
					BasicBlock* not_taken = (cond_val != 0) ? target_false : target_true;

					if (taken) {
						inst->SetOpcode(IROpcode::Jump);
						inst->GetOperands().clear();
						inst->AddOperand(Value::MakeBlockRef(taken));
						if (not_taken) {
							block->RemoveSuccessor(not_taken);
							not_taken->RemovePredecessor(block.get());
						}
						modified = true;
					}
				}
			}
		}
	}
	return modified;
}

// 7. Dead Code Elimination Pass (DCE)
bool DeadCodeEliminationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;
	std::unordered_set<uint32_t> used_vregs;

	// Collect all used VRegs
	for (const auto& block : cfg.GetBlocks()) {
		for (const auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;
			for (const auto& op : inst->GetOperands()) {
				if (op.IsVReg()) used_vregs.insert(op.vreg.id);
			}
		}
	}

	// Eliminate active instructions with destination vreg never used
	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive() || !inst->HasDst() || inst->IsTerminator()) continue;

			if (inst->GetOpcode() != IROpcode::Load && inst->GetOpcode() != IROpcode::Store) {
				if (!used_vregs.count(inst->GetDst().id)) {
					inst->SetActive(false);
					modified = true;
				}
			}
		}
	}
	return modified;
}

// 8. Dead Store Elimination Pass (DSE)
bool DeadStoreEliminationPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		IRInstruction* last_store = nullptr;
		Value last_store_addr{};

		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			if (inst->GetOpcode() == IROpcode::Store) {
				const auto& ops = inst->GetOperands();
				if (ops.size() >= 2) {
					if (last_store && last_store_addr.IsMemoryRef() && ops[1].IsMemoryRef()) {
						if (last_store_addr.mem_ref.base == ops[1].mem_ref.base &&
						    last_store_addr.mem_ref.disp == ops[1].mem_ref.disp) {
							last_store->SetActive(false);
							modified = true;
						}
					}
					last_store = inst.get();
					last_store_addr = ops[1];
				}
			} else if (inst->GetOpcode() == IROpcode::Load || inst->IsTerminator()) {
				last_store = nullptr;
			}
		}
	}
	return modified;
}

// 9. Register Coalescing Pass
bool RegisterCoalescingPass::Run(ControlFlowGraph& cfg, const DominatorTree& /*dom_tree*/) {
	bool modified = false;

	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			if (inst->GetOpcode() == IROpcode::Nop && inst->HasDst()) {
				const auto& ops = inst->GetOperands();
				if (ops.size() == 1 && ops[0].IsVReg()) {
					inst->SetActive(false);
					modified = true;
				}
			}
		}
	}
	return modified;
}

PassManager PassManager::CreateDefaultPipeline() {
	PassManager pm;
	pm.AddPass(std::make_unique<ConstantPropagationPass>());
	pm.AddPass(std::make_unique<ConstantFoldingPass>());
	pm.AddPass(std::make_unique<CopyPropagationPass>());
	pm.AddPass(std::make_unique<AlgebraicSimplificationPass>());
	pm.AddPass(std::make_unique<CommonSubexpressionEliminationPass>());
	pm.AddPass(std::make_unique<BranchSimplificationPass>());
	pm.AddPass(std::make_unique<DeadCodeEliminationPass>());
	pm.AddPass(std::make_unique<DeadStoreEliminationPass>());
	pm.AddPass(std::make_unique<RegisterCoalescingPass>());
	return pm;
}

bool PassManager::RunAll(ControlFlowGraph& cfg) {
	DominatorTree dom_tree;
	dom_tree.Build(cfg);

	bool modified = false;
	bool step_changed = true;
	int iterations = 0;

	while (step_changed && iterations < 10) {
		step_changed = false;
		for (const auto& pass : m_passes) {
			if (pass->Run(cfg, dom_tree)) {
				step_changed = true;
				modified = true;
			}
		}
		if (step_changed) dom_tree.Build(cfg);
		iterations++;
	}

	return modified;
}

} // namespace Loader::Recompiler
