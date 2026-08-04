// irOptimizationPasses.h
//
// Target-Independent Optimization Passes & PassManager for Target-Independent IR.

#ifndef LOADER_RECOMPILER_IR_OPTIMIZATION_PASSES_H
#define LOADER_RECOMPILER_IR_OPTIMIZATION_PASSES_H

#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/irDominatorTree.h"

#include <memory>
#include <vector>

namespace Loader::Recompiler {

class IIROptimizationPass {
public:
	virtual ~IIROptimizationPass() = default;
	[[nodiscard]] virtual const char* GetName() const noexcept = 0;
	virtual bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) = 0;
};

class ConstantPropagationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "ConstantPropagation"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class ConstantFoldingPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "ConstantFolding"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class CopyPropagationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "CopyPropagation"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class AlgebraicSimplificationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "AlgebraicSimplification"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class CommonSubexpressionEliminationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "CommonSubexpressionElimination"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class BranchSimplificationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "BranchSimplification"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class DeadCodeEliminationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "DeadCodeElimination"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class DeadStoreEliminationPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "DeadStoreElimination"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class RegisterCoalescingPass : public IIROptimizationPass {
public:
	const char* GetName() const noexcept override { return "RegisterCoalescing"; }
	bool Run(ControlFlowGraph& cfg, const DominatorTree& dom_tree) override;
};

class PassManager {
public:
	PassManager() = default;
	~PassManager() = default;

	PassManager(const PassManager&) = delete;
	PassManager& operator=(const PassManager&) = delete;

	PassManager(PassManager&&) noexcept = default;
	PassManager& operator=(PassManager&&) noexcept = default;

	static PassManager CreateDefaultPipeline();

	void AddPass(std::unique_ptr<IIROptimizationPass> pass) {
		if (pass) m_passes.push_back(std::move(pass));
	}

	bool RunAll(ControlFlowGraph& cfg);

private:
	std::vector<std::unique_ptr<IIROptimizationPass>> m_passes;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_IR_OPTIMIZATION_PASSES_H
