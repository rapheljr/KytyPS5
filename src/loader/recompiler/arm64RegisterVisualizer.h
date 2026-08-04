// arm64RegisterVisualizer.h
//
// Register Allocation Timeline, Spill/Reload & Pressure Heatmap Visualizer.

#ifndef LOADER_RECOMPILER_ARM64_REGISTER_VISUALIZER_H
#define LOADER_RECOMPILER_ARM64_REGISTER_VISUALIZER_H

#include "loader/recompiler/arm64LinearScanAllocator.h"

#include <string>

namespace Loader::Recompiler {

class RegisterAllocationVisualizer {
public:
	RegisterAllocationVisualizer() = default;
	~RegisterAllocationVisualizer() = default;

	static std::string RenderAsciiTimeline(const std::vector<LiveInterval>& intervals);
	static std::string RenderPressureHeatmap(const std::vector<LiveInterval>& intervals);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_REGISTER_VISUALIZER_H
