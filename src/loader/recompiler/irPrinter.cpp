// irPrinter.cpp
//
// IR Verifier, Human-Readable Printer & Graphviz (.dot) Exporter for Target-Independent IR.

#include "loader/recompiler/irPrinter.h"

#include <sstream>
#include <unordered_map>

namespace Loader::Recompiler {

// IRVerifier Implementation
IRVerifier::VerificationResult IRVerifier::Verify(const ControlFlowGraph& cfg, const DominatorTree& dom_tree) {
	VerificationResult result;
	std::unordered_map<uint32_t, BasicBlock*> def_blocks;

	for (const auto& block : cfg.GetBlocks()) {
		BasicBlock* bb = block.get();

		// Check predecessor/successor symmetry
		for (BasicBlock* succ : bb->GetSuccessors()) {
			if (!succ->HasPredecessor(bb)) {
				result.valid = false;
				result.errors.push_back("CFG Edge Asymmetry: " + bb->GetLabel() + " -> " + succ->GetLabel());
			}
		}

		for (const auto& inst : bb->GetInstructions()) {
			if (!inst->IsActive()) continue;

			// Check single definition invariant
			if (inst->HasDst()) {
				uint32_t vid = inst->GetDst().id;
				if (def_blocks.count(vid)) {
					result.valid = false;
					result.errors.push_back("Multiple definitions for v" + std::to_string(vid) + " in " + bb->GetLabel());
				}
				def_blocks[vid] = bb;
			}

			// Check operand dominance
			for (const auto& op : inst->GetOperands()) {
				if (op.IsVReg()) {
					auto it = def_blocks.find(op.vreg.id);
					if (it != def_blocks.end()) {
						BasicBlock* def_bb = it->second;
						if (def_bb != bb && !dom_tree.Dominates(def_bb, bb)) {
							result.valid = false;
							result.errors.push_back("Operand v" + std::to_string(op.vreg.id) + " used in " +
							                        bb->GetLabel() + " is not dominated by definition in " + def_bb->GetLabel());
						}
					}
				}
			}
		}
	}

	return result;
}

// IRPrinter Implementation
std::string IRPrinter::Print(const ControlFlowGraph& cfg) {
	std::stringstream ss;
	ss << "; ControlFlowGraph Entry: " << (cfg.GetEntryBlock() ? cfg.GetEntryBlock()->GetLabel() : "None") << "\n";
	for (const auto& block : cfg.GetBlocks()) {
		ss << PrintBlock(*block.get());
	}
	return ss.str();
}

std::string IRPrinter::PrintBlock(const BasicBlock& block) {
	std::stringstream ss;
	ss << block.GetLabel() << ":\n";
	for (const auto& inst : block.GetInstructions()) {
		if (inst->IsActive()) {
			ss << "  " << PrintInstruction(*inst.get()) << "\n";
		}
	}
	return ss.str();
}

std::string IRPrinter::PrintInstruction(const IRInstruction& inst) {
	std::stringstream ss;
	if (inst.HasDst()) {
		ss << "v" << inst.GetDst().id << " = ";
	}

	switch (inst.GetOpcode()) {
		case IROpcode::Nop:        ss << "nop"; break;
		case IROpcode::Add:        ss << "add"; break;
		case IROpcode::Sub:        ss << "sub"; break;
		case IROpcode::Mul:        ss << "mul"; break;
		case IROpcode::SDiv:       ss << "sdiv"; break;
		case IROpcode::UDiv:       ss << "udiv"; break;
		case IROpcode::And:        ss << "and"; break;
		case IROpcode::Or:         ss << "or"; break;
		case IROpcode::Xor:        ss << "xor"; break;
		case IROpcode::Shl:        ss << "shl"; break;
		case IROpcode::LShr:       ss << "lshr"; break;
		case IROpcode::AShr:       ss << "ashr"; break;
		case IROpcode::Rol:        ss << "rol"; break;
		case IROpcode::Ror:        ss << "ror"; break;
		case IROpcode::Neg:        ss << "neg"; break;
		case IROpcode::Not:        ss << "not"; break;
		case IROpcode::Load:       ss << "load"; break;
		case IROpcode::Store:      ss << "store"; break;
		case IROpcode::Jump:       ss << "jump"; break;
		case IROpcode::BranchCond: ss << "branch_cond"; break;
		case IROpcode::Return:     ss << "return"; break;
		case IROpcode::Phi:        ss << "phi"; break;
		case IROpcode::Select:     ss << "select"; break;
		default:                   ss << "op_" << static_cast<int>(inst.GetOpcode()); break;
	}

	const auto& ops = inst.GetOperands();
	for (size_t i = 0; i < ops.size(); ++i) {
		ss << (i == 0 ? " " : ", ");
		const auto& op = ops[i];
		if (op.IsVReg()) ss << "v" << op.vreg.id;
		else if (op.IsImmInt()) ss << op.imm_int;
		else if (op.IsImmFloat()) ss << op.imm_float;
		else if (op.IsBlockRef() && op.block_ref) ss << op.block_ref->GetLabel();
		else if (op.IsMemoryRef()) ss << "[v" << op.mem_ref.base.id << " + " << op.mem_ref.disp << "]";
	}

	return ss.str();
}

// GraphvizExporter Implementation
std::string GraphvizExporter::ExportCFG(const ControlFlowGraph& cfg) {
	std::stringstream ss;
	ss << "digraph CFG {\n";
	ss << "  node [shape=box, fontname=\"Courier\"];\n";

	for (const auto& block : cfg.GetBlocks()) {
		BasicBlock* bb = block.get();
		ss << "  \"" << bb->GetLabel() << "\" [label=\"" << bb->GetLabel() << "\\n"
		   << bb->GetInstructions().size() << " insts\"];\n";

		for (BasicBlock* succ : bb->GetSuccessors()) {
			ss << "  \"" << bb->GetLabel() << "\" -> \"" << succ->GetLabel() << "\";\n";
		}
	}
	ss << "}\n";
	return ss.str();
}

std::string GraphvizExporter::ExportDominatorTree(const ControlFlowGraph& cfg, const DominatorTree& dom_tree) {
	std::stringstream ss;
	ss << "digraph DominatorTree {\n";
	ss << "  node [shape=ellipse, fontname=\"Courier\"];\n";

	for (const auto& block : cfg.GetBlocks()) {
		BasicBlock* bb = block.get();
		BasicBlock* idom = dom_tree.GetIDom(bb);
		if (idom) {
			ss << "  \"" << idom->GetLabel() << "\" -> \"" << bb->GetLabel() << "\";\n";
		}
	}
	ss << "}\n";
	return ss.str();
}

} // namespace Loader::Recompiler
