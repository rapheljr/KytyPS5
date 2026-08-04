// ShaderOptPasses.cpp
//
// 10 Optimization Pass Implementations for Phase L.

#include "graphics/shader/recompiler/opt/ShaderOptPasses.h"

#include <algorithm>
#include <cmath>

namespace Libs::Graphics::ShaderRecompiler::Opt {

// ─── 1. Constant Folding & Propagation Pass ───────────────────────────────────

bool PassConstantFoldingAndPropagation::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (auto& inst : insts) {
		if (!inst.active) continue;

		// Fold immediate integer add: dst = const_a + const_b
		if (inst.opcode == IROpcode::Add && inst.src0.is_imm && inst.src1.is_imm) {
			inst.opcode = IROpcode::Mov;
			inst.src0.imm_u32 = inst.src0.imm_u32 + inst.src1.imm_u32;
			inst.src1.is_imm = false;
			stats.constants_folded++;
			stats.instructions_eliminated++;
			modified = true;
		}
		// Fold immediate integer sub: dst = const_a - const_b
		else if (inst.opcode == IROpcode::Sub && inst.src0.is_imm && inst.src1.is_imm) {
			inst.opcode = IROpcode::Mov;
			inst.src0.imm_u32 = inst.src0.imm_u32 - inst.src1.imm_u32;
			inst.src1.is_imm = false;
			stats.constants_folded++;
			stats.instructions_eliminated++;
			modified = true;
		}
		// Fold immediate integer mul: dst = const_a * const_b
		else if (inst.opcode == IROpcode::Mul && inst.src0.is_imm && inst.src1.is_imm) {
			inst.opcode = IROpcode::Mov;
			inst.src0.imm_u32 = inst.src0.imm_u32 * inst.src1.imm_u32;
			inst.src1.is_imm = false;
			stats.constants_folded++;
			stats.instructions_eliminated++;
			modified = true;
		}
		// Fold immediate float add: dst = const_f0 + const_f1
		else if (inst.opcode == IROpcode::FAdd && inst.src0.is_imm && inst.src1.is_imm) {
			inst.opcode = IROpcode::FMov;
			inst.src0.imm_f32 = inst.src0.imm_f32 + inst.src1.imm_f32;
			inst.src1.is_imm = false;
			stats.constants_folded++;
			stats.instructions_eliminated++;
			modified = true;
		}
		// Fold immediate bitwise AND: dst = const_a & const_b
		else if (inst.opcode == IROpcode::And && inst.src0.is_imm && inst.src1.is_imm) {
			inst.opcode = IROpcode::Mov;
			inst.src0.imm_u32 = inst.src0.imm_u32 & inst.src1.imm_u32;
			inst.src1.is_imm = false;
			stats.constants_folded++;
			stats.instructions_eliminated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 2. Dead Code Elimination (DCE) Pass ─────────────────────────────────────

bool PassDeadCodeElimination::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	std::vector<uint32_t> use_counts(ir.GetMaxRegisterId() + 1, 0);

	// Count uses
	for (const auto& inst : insts) {
		if (!inst.active) continue;
		if (!inst.src0.is_imm) use_counts[inst.src0.reg_id]++;
		if (inst.opcode != IROpcode::Mov && inst.opcode != IROpcode::FMov && inst.opcode != IROpcode::Nop) {
			if (!inst.src1.is_imm) use_counts[inst.src1.reg_id]++;
		}
	}


	// Eliminate instructions whose dst reg has 0 uses (unless side-effecting like store/export/barrier)
	for (auto& inst : insts) {
		if (!inst.active) continue;
		if (inst.is_side_effecting) continue;

		if (use_counts[inst.dst_reg_id] == 0) {
			inst.active = false;
			stats.instructions_eliminated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 3. Copy Propagation Pass ─────────────────────────────────────────────────

bool PassCopyPropagation::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	std::vector<IROperand> copy_map(ir.GetMaxRegisterId() + 1);
	std::vector<bool> has_copy(ir.GetMaxRegisterId() + 1, false);

	for (const auto& inst : insts) {
		if (!inst.active) continue;

		if (inst.opcode == IROpcode::Mov || inst.opcode == IROpcode::FMov) {
			copy_map[inst.dst_reg_id] = inst.src0;
			has_copy[inst.dst_reg_id] = true;
		}
	}

	for (auto& inst : insts) {
		if (!inst.active) continue;

		if (!inst.src0.is_imm && has_copy[inst.src0.reg_id]) {
			inst.src0 = copy_map[inst.src0.reg_id];
			stats.copies_propagated++;
			modified = true;
		}
		if (!inst.src1.is_imm && has_copy[inst.src1.reg_id]) {
			inst.src1 = copy_map[inst.src1.reg_id];
			stats.copies_propagated++;
			modified = true;
		}
		if (!inst.src2.is_imm && has_copy[inst.src2.reg_id]) {
			inst.src2 = copy_map[inst.src2.reg_id];
			stats.copies_propagated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 4. Common Subexpression Elimination (CSE) Pass ─────────────────────────

bool PassCommonSubexpressionElimination::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (size_t i = 0; i < insts.size(); ++i) {
		if (!insts[i].active || insts[i].is_side_effecting) continue;

		for (size_t j = i + 1; j < insts.size(); ++j) {
			if (!insts[j].active || insts[j].is_side_effecting) continue;

			if (insts[i].opcode == insts[j].opcode &&
			    insts[i].src0 == insts[j].src0 &&
			    insts[i].src1 == insts[j].src1 &&
			    insts[i].src2 == insts[j].src2) {

				// Match found: replace inst j with MOV from inst i's dst_reg
				insts[j].opcode = (insts[i].opcode == IROpcode::FAdd || insts[i].opcode == IROpcode::FMul) ? IROpcode::FMov : IROpcode::Mov;
				insts[j].src0.is_imm = false;
				insts[j].src0.reg_id = insts[i].dst_reg_id;
				insts[j].src1.is_imm = false;
				insts[j].src2.is_imm = false;

				stats.instructions_eliminated++;
				modified = true;
			}
		}
	}

	return modified;
}

// ─── 5. Peephole Optimization Pass ───────────────────────────────────────────

bool PassPeepholeOptimization::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (size_t i = 0; i + 1 < insts.size(); ++i) {
		if (!insts[i].active || !insts[i + 1].active) continue;

		// Mov R1, R0 followed by Mov R2, R1 -> Mov R2, R0
		if (insts[i].opcode == IROpcode::Mov && insts[i + 1].opcode == IROpcode::Mov) {
			if (!insts[i + 1].src0.is_imm && insts[i + 1].src0.reg_id == insts[i].dst_reg_id) {
				insts[i + 1].src0 = insts[i].src0;
				stats.instructions_eliminated++;
				modified = true;
			}
		}
	}

	return modified;
}

// ─── 6. Algebraic Simplification Pass ─────────────────────────────────────────

bool PassAlgebraicSimplification::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (auto& inst : insts) {
		if (!inst.active) continue;

		// x + 0 -> x
		if (inst.opcode == IROpcode::Add && inst.src1.is_imm && inst.src1.imm_u32 == 0) {
			inst.opcode = IROpcode::Mov;
			inst.src1.is_imm = false;
			stats.instructions_eliminated++;
			modified = true;
		}
		// x * 1 -> x
		else if (inst.opcode == IROpcode::Mul && inst.src1.is_imm && inst.src1.imm_u32 == 1) {
			inst.opcode = IROpcode::Mov;
			inst.src1.is_imm = false;
			stats.instructions_eliminated++;
			modified = true;
		}
		// x * 0 -> 0
		else if (inst.opcode == IROpcode::Mul && inst.src1.is_imm && inst.src1.imm_u32 == 0) {
			inst.opcode = IROpcode::Mov;
			inst.src0.is_imm = true;
			inst.src0.imm_u32 = 0;
			inst.src1.is_imm = false;
			stats.instructions_eliminated++;
			modified = true;
		}
		// x ^ x -> 0
		else if (inst.opcode == IROpcode::Xor && !inst.src0.is_imm && !inst.src1.is_imm && inst.src0.reg_id == inst.src1.reg_id) {
			inst.opcode = IROpcode::Mov;
			inst.src0.is_imm = true;
			inst.src0.imm_u32 = 0;
			inst.src1.is_imm = false;
			stats.instructions_eliminated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 7. Branch Simplification Pass ───────────────────────────────────────────

bool PassBranchSimplification::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (auto& inst : insts) {
		if (!inst.active) continue;

		// Branch to next immediate instruction (nop branch)
		if (inst.opcode == IROpcode::Branch && inst.src0.is_imm && inst.src0.imm_u32 == inst.target_label_id) {
			inst.active = false;
			stats.instructions_eliminated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 8. SSA Cleanup Pass ─────────────────────────────────────────────────────

bool PassSsaCleanup::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	for (auto& inst : insts) {
		if (!inst.active) continue;

		// Clean up redundant self-assignments (Mov R1, R1)
		if ((inst.opcode == IROpcode::Mov || inst.opcode == IROpcode::FMov) &&
		    !inst.src0.is_imm && inst.src0.reg_id == inst.dst_reg_id) {
			inst.active = false;
			stats.instructions_eliminated++;
			modified = true;
		}
	}

	return modified;
}

// ─── 9. Register Coalescing Pass ──────────────────────────────────────────────

bool PassRegisterCoalescing::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	uint32_t next_reg_id = 0;
	std::vector<uint32_t> remap_table(ir.GetMaxRegisterId() + 1, 0xFFFFFFFF);

	for (auto& inst : insts) {
		if (!inst.active) continue;

		if (remap_table[inst.dst_reg_id] == 0xFFFFFFFF) {
			remap_table[inst.dst_reg_id] = next_reg_id++;
			stats.registers_coalesced++;
			modified = true;
		}
		inst.dst_reg_id = remap_table[inst.dst_reg_id];

		if (!inst.src0.is_imm && remap_table[inst.src0.reg_id] != 0xFFFFFFFF) {
			inst.src0.reg_id = remap_table[inst.src0.reg_id];
		}
		if (!inst.src1.is_imm && remap_table[inst.src1.reg_id] != 0xFFFFFFFF) {
			inst.src1.reg_id = remap_table[inst.src1.reg_id];
		}
		if (!inst.src2.is_imm && remap_table[inst.src2.reg_id] != 0xFFFFFFFF) {
			inst.src2.reg_id = remap_table[inst.src2.reg_id];
		}
	}

	if (next_reg_id > 0) {
		ir.SetMaxRegisterId(next_reg_id - 1);
	}

	return modified;
}

// ─── 10. Instruction Scheduling Pass ─────────────────────────────────────────

bool PassInstructionScheduling::Run(ShaderIR& ir, PassStats& stats) {
	bool modified = false;
	auto& insts = ir.GetInstructions();

	// Stable sort independent instructions to group memory loads ahead of arithmetic
	std::stable_sort(insts.begin(), insts.end(), [](const IRInstruction& a, const IRInstruction& b) {
		if (!a.active || !b.active) return false;
		// Prioritize memory loads over ALU arithmetic
		bool a_is_load = (a.opcode == IROpcode::LoadStorage || a.opcode == IROpcode::LoadUniform);
		bool b_is_load = (b.opcode == IROpcode::LoadStorage || b.opcode == IROpcode::LoadUniform);
		if (a_is_load != b_is_load) {
			return a_is_load > b_is_load;
		}
		return false;
	});

	(void)stats;
	return modified;
}

} // namespace Libs::Graphics::ShaderRecompiler::Opt
