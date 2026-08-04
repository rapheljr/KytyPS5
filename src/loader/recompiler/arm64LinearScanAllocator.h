// arm64LinearScanAllocator.h
//
// Production-Quality Linear Scan Register Allocator for ARM64 Backend.

#ifndef LOADER_RECOMPILER_ARM64_LINEAR_SCAN_ALLOCATOR_H
#define LOADER_RECOMPILER_ARM64_LINEAR_SCAN_ALLOCATOR_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Loader::Recompiler {

struct LiveInterval {
	VirtualReg           vreg{};
	uint32_t             start              = 0;
	uint32_t             end                = 0;
	bool                 is_vector          = false;
	int8_t               assigned_phys_reg  = -1;
	bool                 is_spilled         = false;
	int32_t              spill_slot_offset  = -1;
	std::vector<uint32_t> uses;

	bool Overlaps(const LiveInterval& other) const noexcept {
		return !(end < other.start || start > other.end);
	}
};

class LinearScanAllocator {
public:
	LinearScanAllocator() = default;
	~LinearScanAllocator() = default;

	KYTY_CLASS_NO_COPY(LinearScanAllocator);

	struct AllocationResult {
		bool success = true;
		uint32_t total_spill_bytes = 0;
		std::unordered_set<Arm64Reg> used_callee_saved_gprs;
		std::unordered_set<uint8_t>  used_callee_saved_vecs;
	};

	AllocationResult Allocate(ControlFlowGraph& cfg);

	[[nodiscard]] const std::vector<LiveInterval>& GetIntervals() const noexcept { return m_intervals; }
	[[nodiscard]] const LiveInterval* GetIntervalForVReg(uint32_t vreg_id) const noexcept;

private:
	void BuildLiveIntervals(const ControlFlowGraph& cfg);
	void CoalesceRegisters(ControlFlowGraph& cfg);
	void InsertSpillsAndReloads(ControlFlowGraph& cfg);

	int8_t AllocateFreeRegister(bool is_vector, const std::unordered_set<int8_t>& occupied);
	void ExpireOldIntervals(uint32_t current_point, std::vector<LiveInterval*>& active, std::unordered_set<int8_t>& occupied);

	std::vector<LiveInterval>                          m_intervals;
	std::unordered_map<uint32_t, size_t>               m_vreg_to_interval_idx;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_LINEAR_SCAN_ALLOCATOR_H
