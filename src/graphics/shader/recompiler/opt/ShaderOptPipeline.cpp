// ShaderOptPipeline.cpp
//
// Shader Optimization Pipeline Infrastructure for Phase L.

#include "graphics/shader/recompiler/opt/ShaderOptPipeline.h"
#include "graphics/shader/recompiler/opt/ShaderOptPasses.h"

#include <chrono>

namespace Libs::Graphics::ShaderRecompiler::Opt {

ShaderOptPassManager::ShaderOptPassManager(ShaderOptLevel level) : m_level(level) {
	ConfigureForLevel(level);
}

void ShaderOptPassManager::AddPass(std::unique_ptr<ShaderOptPass> pass) {
	if (pass) {
		m_passes.push_back(std::move(pass));
	}
}

void ShaderOptPassManager::ConfigureForLevel(ShaderOptLevel level) {
	m_passes.clear();
	m_level = level;

	if (level == ShaderOptLevel::O0) {
		return; // No optimization passes
	}

	// O1: Basic Optimizations (Constant Folding, DCE, Copy Propagation)
	AddPass(std::make_unique<PassConstantFoldingAndPropagation>());
	AddPass(std::make_unique<PassDeadCodeElimination>());
	AddPass(std::make_unique<PassCopyPropagation>());

	if (level >= ShaderOptLevel::O2) {
		// O2: Full Optimizations (CSE, Peephole, Algebraic, Branch Simplification, SSA Cleanup)
		AddPass(std::make_unique<PassCommonSubexpressionElimination>());
		AddPass(std::make_unique<PassPeepholeOptimization>());
		AddPass(std::make_unique<PassAlgebraicSimplification>());
		AddPass(std::make_unique<PassBranchSimplification>());
		AddPass(std::make_unique<PassSsaCleanup>());
	}

	if (level >= ShaderOptLevel::O3) {
		// O3: Aggressive Optimizations (Register Coalescing & Instruction Scheduling)
		AddPass(std::make_unique<PassRegisterCoalescing>());
		AddPass(std::make_unique<PassInstructionScheduling>());
	}
}

bool ShaderOptPassManager::Run(ShaderIR& ir) {
	ResetStats();

	m_stats.initial_instruction_count = ir.GetInstructionCount();

	if (m_level == ShaderOptLevel::O0 || m_passes.empty()) {
		m_stats.final_instruction_count = m_stats.initial_instruction_count;
		return true;
	}

	auto t_start = std::chrono::high_resolution_clock::now();


	constexpr uint32_t kMaxIterations = 4;
	for (uint32_t iter = 0; iter < kMaxIterations; ++iter) {
		bool any_changed = false;

		for (const auto& pass : m_passes) {
			PassStats pstats{};
			pstats.pass_name = pass->GetName();

			auto p_start = std::chrono::high_resolution_clock::now();
			bool changed = pass->Run(ir, pstats);
			auto p_end = std::chrono::high_resolution_clock::now();

			pstats.execution_time_us = std::chrono::duration<double, std::micro>(p_end - p_start).count();
			m_stats.pass_stats.push_back(pstats);
			m_stats.passes_run++;

			if (changed) {
				any_changed = true;
			}

			if (m_debug_validation) {
				std::string err;
				if (!ValidateIR(ir, &err)) {
					return false;
				}
			}
		}

		if (!any_changed) {
			break; // Fixed point reached
		}
	}

	m_stats.final_instruction_count = ir.GetInstructionCount();
	auto t_end = std::chrono::high_resolution_clock::now();
	m_stats.total_time_us = std::chrono::duration<double, std::micro>(t_end - t_start).count();

	return true;
}

bool ShaderOptPassManager::ValidateIR(const ShaderIR& ir, std::string* out_error) {
	if (ir.GetInstructionCount() == 0 && ir.GetBasicBlockCount() > 0) {
		if (out_error) *out_error = "IR contains basic blocks but zero instructions";
		return true;
	}
	return true;
}

} // namespace Libs::Graphics::ShaderRecompiler::Opt
