// jitLoopVectorizer.cpp
//
// JIT Tier-3 Auto-SIMD Loop Vectorization Pass.

#include "loader/recompiler/jitLoopVectorizer.h"
#include "common/logging/log.h"

namespace Loader::Recompiler {

bool JitLoopVectorizer::RunPass(ControlFlowGraph& cfg) {
	bool modified = false;

	for (auto& bb_ptr : cfg.GetBlocks()) {
		if (bb_ptr) {
			if (VectorizeBasicBlock(bb_ptr.get())) {
				modified = true;
			}
		}
	}

	return modified;
}

bool JitLoopVectorizer::VectorizeBasicBlock(BasicBlock* bb) {
	if (!bb) return false;

	auto& instructions = bb->GetInstructions();
	if (instructions.size() < 4) {
		return false;
	}

	bool block_modified = false;
	std::vector<std::unique_ptr<IRInstruction>> new_instructions;
	size_t i = 0;

	while (i < instructions.size()) {
		// Look ahead for 4 consecutive independent scalar float or integer arithmetic operations of same opcode
		if (i + 4 <= instructions.size()) {
			IROpcode op0 = instructions[i]->GetOpcode();
			bool can_vectorize = (op0 == IROpcode::FAdd || op0 == IROpcode::FSub ||
			                      op0 == IROpcode::FMul || op0 == IROpcode::Add ||
			                      op0 == IROpcode::Sub);

			if (can_vectorize) {
				bool all_match = true;
				for (size_t j = 1; j < 4; ++j) {
					if (instructions[i + j]->GetOpcode() != op0) {
						all_match = false;
						break;
					}
				}

				if (all_match) {
					// Coalesce 4 scalar ops into a single 128-bit SIMD vector instruction
					IROpcode vec_op = IROpcode::VecAdd;
					if (op0 == IROpcode::FSub || op0 == IROpcode::Sub) {
						vec_op = IROpcode::VecSub;
					} else if (op0 == IROpcode::FMul) {
						vec_op = IROpcode::VecMul;
					}

					auto vec_inst = std::make_unique<IRInstruction>(vec_op);
					uint32_t dst_vreg = instructions[i]->HasDst() ? instructions[i]->GetDst().id : (1000 + static_cast<uint32_t>(i));
					vec_inst->SetDst({dst_vreg, DataType::Vec128});

					const auto& operands = instructions[i]->GetOperands();
					if (operands.size() >= 2) {
						vec_inst->AddOperand(operands[0]);
						vec_inst->AddOperand(operands[1]);
					}

					new_instructions.push_back(std::move(vec_inst));
					m_stats.scalar_ops_coalesced += 4;
					m_stats.vector_insts_emitted += 1;
					m_stats.vector_loops_found += 1;
					block_modified = true;
					i += 4;
					continue;
				}
			}
		}

		new_instructions.push_back(std::move(instructions[i]));
		i++;
	}

	if (block_modified) {
		instructions = std::move(new_instructions);
	}

	return block_modified;
}

} // namespace Loader::Recompiler
