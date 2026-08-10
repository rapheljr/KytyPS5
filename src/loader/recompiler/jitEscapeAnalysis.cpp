// jitEscapeAnalysis.cpp
//
// JIT Tier-2 Escape Analysis & Stack Scalar Replacement Implementation.

#include "loader/recompiler/jitEscapeAnalysis.h"

namespace Loader::Recompiler {

JitEscapeAnalysis::JitEscapeAnalysis() = default;

bool JitEscapeAnalysis::RunPass(ControlFlowGraph& cfg) {
	bool modified = false;

	for (auto& bb : cfg.GetBlocks()) {
		// Map displacement offset -> last stored VirtualReg
		std::unordered_map<int32_t, VirtualReg> stack_slot_map;

		for (auto& instr : bb->GetInstructions()) {
			if (!instr->IsActive()) continue;

			if (instr->GetOpcode() == IROpcode::Store) {
				const auto& ops = instr->GetOperands();
				// Check if memory ref to stack displacement
				if (ops.size() >= 2 && ops[0].kind == Value::Kind::MemoryRef) {
					int32_t disp = ops[0].mem_ref.disp;
					if (ops[1].kind == Value::Kind::VReg) {
						stack_slot_map[disp] = ops[1].vreg;
						m_stats.stack_slots_tracked++;
					}
				}
			} else if (instr->GetOpcode() == IROpcode::Load) {
				const auto& ops = instr->GetOperands();
				if (!ops.empty() && ops[0].kind == Value::Kind::MemoryRef && instr->HasDst()) {
					int32_t disp = ops[0].mem_ref.disp;
					auto it = stack_slot_map.find(disp);
					if (it != stack_slot_map.end()) {
						// Forward stored vreg directly -> turn Load into Add (vreg, 0)
						instr->SetOpcode(IROpcode::Add);
						instr->GetOperands().clear();
						instr->AddOperand(Value::MakeVReg(it->second));
						instr->AddOperand(Value::MakeImmInt(0));
						m_stats.stack_loads_forwarded++;
						modified = true;
					}
				}
			}
		}
	}

	return modified;
}

void JitEscapeAnalysis::Reset() noexcept {
	m_stats = {};
}

} // namespace Loader::Recompiler
