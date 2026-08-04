// arm64LinearScanAllocator.cpp
//
// Production-Quality Linear Scan Register Allocator for ARM64 Backend.

#include "loader/recompiler/arm64LinearScanAllocator.h"

#include <algorithm>

namespace Loader::Recompiler {

// AAPCS64 Allocatable Pools
static const int8_t kGprCallerSaved[] = {9, 10, 11, 12, 13, 14, 15};
static const int8_t kGprCalleeSaved[] = {19, 20, 21, 22, 23, 24, 25, 26, 27, 28};

static const int8_t kVecCallerSaved[] = {0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31};
static const int8_t kVecCalleeSaved[] = {8, 9, 10, 11, 12, 13, 14, 15};

const LiveInterval* LinearScanAllocator::GetIntervalForVReg(uint32_t vreg_id) const noexcept {
	auto it = m_vreg_to_interval_idx.find(vreg_id);
	if (it != m_vreg_to_interval_idx.end()) {
		return &m_intervals[it->second];
	}
	return nullptr;
}

void LinearScanAllocator::BuildLiveIntervals(const ControlFlowGraph& cfg) {
	m_intervals.clear();
	m_vreg_to_interval_idx.clear();

	uint32_t point = 0;
	std::unordered_map<uint32_t, LiveInterval> interval_map;

	std::vector<BasicBlock*> rpo = cfg.ComputeReversePostOrder();

	for (BasicBlock* block : rpo) {
		for (const auto& inst : block->GetInstructions()) {
			if (!inst->IsActive()) continue;

			point++;

			// Record uses
			for (const auto& op : inst->GetOperands()) {
				if (op.IsVReg()) {
					uint32_t vid = op.vreg.id;
					auto& li = interval_map[vid];
					li.vreg = op.vreg;
					li.is_vector = (op.vreg.type == DataType::Float32 || op.vreg.type == DataType::Float64 || op.vreg.type == DataType::Vec128);
					if (li.start == 0) li.start = point;
					li.end = point;
					li.uses.push_back(point);
				}
			}

			// Record def
			if (inst->HasDst()) {
				uint32_t vid = inst->GetDst().id;
				auto& li = interval_map[vid];
				li.vreg = inst->GetDst();
				li.is_vector = (inst->GetDst().type == DataType::Float32 || inst->GetDst().type == DataType::Float64 || inst->GetDst().type == DataType::Vec128);
				if (li.start == 0) li.start = point;
				li.end = std::max(li.end, point);
				li.uses.push_back(point);
			}
		}
	}

	for (auto& [vid, li] : interval_map) {
		m_vreg_to_interval_idx[vid] = m_intervals.size();
		m_intervals.push_back(li);
	}

	std::sort(m_intervals.begin(), m_intervals.end(), [](const LiveInterval& a, const LiveInterval& b) {
		return a.start < b.start;
	});

	// Rebuild index mapping after sort
	m_vreg_to_interval_idx.clear();
	for (size_t i = 0; i < m_intervals.size(); ++i) {
		m_vreg_to_interval_idx[m_intervals[i].vreg.id] = i;
	}
}

int8_t LinearScanAllocator::AllocateFreeRegister(bool is_vector, const std::unordered_set<int8_t>& occupied) {
	if (!is_vector) {
		// Prefer caller-saved GPRs first
		for (int8_t reg : kGprCallerSaved) {
			if (!occupied.count(reg)) return reg;
		}
		// Then callee-saved GPRs
		for (int8_t reg : kGprCalleeSaved) {
			if (!occupied.count(reg)) return reg;
		}
	} else {
		for (int8_t reg : kVecCallerSaved) {
			if (!occupied.count(reg)) return reg;
		}
		for (int8_t reg : kVecCalleeSaved) {
			if (!occupied.count(reg)) return reg;
		}
	}
	return -1;
}

void LinearScanAllocator::ExpireOldIntervals(uint32_t current_point, std::vector<LiveInterval*>& active, std::unordered_set<int8_t>& occupied) {
	auto it = active.begin();
	while (it != active.end()) {
		if ((*it)->end < current_point) {
			if ((*it)->assigned_phys_reg >= 0) {
				occupied.erase((*it)->assigned_phys_reg);
			}
			it = active.erase(it);
		} else {
			++it;
		}
	}
}

void LinearScanAllocator::CoalesceRegisters(ControlFlowGraph& cfg) {
	// Merge virtual registers across Nop/Copy instructions if intervals do not overlap
	for (const auto& block : cfg.GetBlocks()) {
		for (auto& inst : block->GetInstructions()) {
			if (inst->IsActive() && inst->GetOpcode() == IROpcode::Nop && inst->HasDst()) {
				const auto& ops = inst->GetOperands();
				if (ops.size() == 1 && ops[0].IsVReg()) {
					uint32_t dst_id = inst->GetDst().id;
					uint32_t src_id = ops[0].vreg.id;

					auto* dst_li = const_cast<LiveInterval*>(GetIntervalForVReg(dst_id));
					auto* src_li = const_cast<LiveInterval*>(GetIntervalForVReg(src_id));

					if (dst_li && src_li && !dst_li->Overlaps(*src_li)) {
						dst_li->assigned_phys_reg = src_li->assigned_phys_reg;
					}
				}
			}
		}
	}
}

void LinearScanAllocator::InsertSpillsAndReloads(ControlFlowGraph& cfg) {
	for (const auto& block : cfg.GetBlocks()) {
		auto& insts = block->GetInstructions();
		std::vector<std::unique_ptr<IRInstruction>> new_insts;

		for (auto& inst : insts) {
			if (!inst->IsActive()) {
				new_insts.push_back(std::move(inst));
				continue;
			}

			// Insert Reloads before instruction for spilled operands
			for (const auto& op : inst->GetOperands()) {
				if (op.IsVReg()) {
					const auto* li = GetIntervalForVReg(op.vreg.id);
					if (li && li->is_spilled) {
						auto reload = std::make_unique<IRInstruction>(IROpcode::Load);
						VirtualReg scratch_vr = op.vreg;
						scratch_vr.phys_pin = 15; // Temporarily load into scratch X15/V15
						reload->SetDst(scratch_vr);
						reload->AddOperand(Value::MakeMemory(VirtualReg{31, DataType::Int64}, VirtualReg{0}, 1, li->spill_slot_offset));
						new_insts.push_back(std::move(reload));
					}
				}
			}

			new_insts.push_back(std::move(inst));

			// Insert Spill after instruction for spilled destination
			if (new_insts.back()->HasDst()) {
				uint32_t dst_id = new_insts.back()->GetDst().id;
				const auto* li = GetIntervalForVReg(dst_id);
				if (li && li->is_spilled) {
					auto spill = std::make_unique<IRInstruction>(IROpcode::Store);
					VirtualReg scratch_vr = new_insts.back()->GetDst();
					scratch_vr.phys_pin = 15;
					spill->AddOperand(Value::MakeVReg(scratch_vr));
					spill->AddOperand(Value::MakeMemory(VirtualReg{31, DataType::Int64}, VirtualReg{0}, 1, li->spill_slot_offset));
					new_insts.push_back(std::move(spill));
				}
			}
		}

		insts = std::move(new_insts);
	}
}

LinearScanAllocator::AllocationResult LinearScanAllocator::Allocate(ControlFlowGraph& cfg) {
	AllocationResult result;
	BuildLiveIntervals(cfg);

	std::vector<LiveInterval*> active;
	std::unordered_set<int8_t> occupied_regs;
	int32_t current_spill_offset = 16; // 16-byte stack alignment base

	for (auto& interval : m_intervals) {
		ExpireOldIntervals(interval.start, active, occupied_regs);

		// Handle pinned physical registers
		if (interval.vreg.phys_pin >= 0) {
			interval.assigned_phys_reg = static_cast<int8_t>(Arm64Emitter::MapX86ToArm64Reg(static_cast<X86Reg>(interval.vreg.phys_pin)));
			occupied_regs.insert(interval.assigned_phys_reg);
			active.push_back(&interval);
			continue;
		}

		int8_t free_reg = AllocateFreeRegister(interval.is_vector, occupied_regs);

		if (free_reg >= 0) {
			interval.assigned_phys_reg = free_reg;
			occupied_regs.insert(free_reg);
			active.push_back(&interval);

			// Track callee-saved usage
			if (!interval.is_vector) {
				if (free_reg >= 19 && free_reg <= 28) {
					result.used_callee_saved_gprs.insert(static_cast<Arm64Reg>(free_reg));
				}
			} else {
				if (free_reg >= 8 && free_reg <= 15) {
					result.used_callee_saved_vecs.insert(static_cast<uint8_t>(free_reg));
				}
			}
		} else {
			// Spill Candidate Selection: Select interval with furthest end point
			LiveInterval* spill_cand = active.empty() ? &interval : active.back();
			for (auto* act : active) {
				if (act->end > spill_cand->end) spill_cand = act;
			}

			if (spill_cand->end > interval.end) {
				interval.assigned_phys_reg = spill_cand->assigned_phys_reg;
				spill_cand->assigned_phys_reg = -1;
				spill_cand->is_spilled = true;
				spill_cand->spill_slot_offset = current_spill_offset;
				current_spill_offset += (spill_cand->is_vector ? 16 : 8);

				std::erase(active, spill_cand);
				active.push_back(&interval);
			} else {
				interval.is_spilled = true;
				interval.spill_slot_offset = current_spill_offset;
				current_spill_offset += (interval.is_vector ? 16 : 8);
			}
		}
	}

	CoalesceRegisters(cfg);
	InsertSpillsAndReloads(cfg);

	result.total_spill_bytes = static_cast<uint32_t>((current_spill_offset + 15) & ~15);
	return result;
}

} // namespace Loader::Recompiler
