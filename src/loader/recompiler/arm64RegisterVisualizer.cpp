// arm64RegisterVisualizer.cpp
//
// Register Allocation Timeline, Spill/Reload & Pressure Heatmap Visualizer.

#include "loader/recompiler/arm64RegisterVisualizer.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Loader::Recompiler {

std::string RegisterAllocationVisualizer::RenderAsciiTimeline(const std::vector<LiveInterval>& intervals) {
	std::stringstream ss;
	ss << "========================================================================\n";
	ss << "           ARM64 Linear Scan Register Allocation Timeline              \n";
	ss << "========================================================================\n";

	if (intervals.empty()) {
		ss << "No live intervals.\n";
		return ss.str();
	}

	uint32_t max_point = 0;
	for (const auto& li : intervals) max_point = std::max(max_point, li.end);

	ss << " VReg  | Reg/Spill | Range     | Timeline\n";
	ss << "-------+-----------+-----------+----------------------------------------\n";

	for (const auto& li : intervals) {
		ss << " v" << std::setw(4) << std::left << li.vreg.id << " | ";
		if (li.is_spilled) {
			ss << "[spill:" << std::setw(2) << li.spill_slot_offset << "] | ";
		} else {
			ss << (li.is_vector ? "V" : "X") << std::setw(8) << static_cast<int>(li.assigned_phys_reg) << " | ";
		}
		ss << "[" << std::setw(3) << li.start << ".." << std::setw(3) << li.end << "] | ";

		for (uint32_t p = 1; p <= max_point && p <= 40; ++p) {
			if (p == li.start) ss << "|";
			else if (p == li.end) ss << "]";
			else if (p > li.start && p < li.end) ss << "=";
			else ss << ".";
		}
		ss << "\n";
	}
	return ss.str();
}

std::string RegisterAllocationVisualizer::RenderPressureHeatmap(const std::vector<LiveInterval>& intervals) {
	std::stringstream ss;
	ss << "=== Register Pressure Heatmap ===\n";

	if (intervals.empty()) return ss.str();

	uint32_t max_point = 0;
	for (const auto& li : intervals) max_point = std::max(max_point, li.end);

	std::vector<uint32_t> gpr_pressure(max_point + 1, 0);
	std::vector<uint32_t> vec_pressure(max_point + 1, 0);

	for (const auto& li : intervals) {
		for (uint32_t p = li.start; p <= li.end && p <= max_point; ++p) {
			if (li.is_vector) vec_pressure[p]++;
			else gpr_pressure[p]++;
		}
	}

	ss << "Point | GPR Active | SIMD Active | Heatmap Graph\n";
	ss << "------+------------+-------------+------------------------------------\n";
	for (uint32_t p = 1; p <= max_point; ++p) {
		ss << " " << std::setw(4) << p << " | "
		   << std::setw(10) << gpr_pressure[p] << " | "
		   << std::setw(11) << vec_pressure[p] << " | ";

		for (uint32_t i = 0; i < gpr_pressure[p]; ++i) ss << "#";
		for (uint32_t i = 0; i < vec_pressure[p]; ++i) ss << "*";
		ss << "\n";
	}
	return ss.str();
}

} // namespace Loader::Recompiler
