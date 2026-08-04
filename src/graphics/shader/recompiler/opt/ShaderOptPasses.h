// ShaderOptPasses.h
//
// 10 Optimization Pass Implementations for Phase L.

#ifndef GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PASSES_H
#define GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PASSES_H

#include "graphics/shader/recompiler/opt/ShaderOptPipeline.h"

namespace Libs::Graphics::ShaderRecompiler::Opt {

// 1. Constant Folding & Constant Propagation
class PassConstantFoldingAndPropagation final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "ConstantFoldingAndPropagation"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 2. Dead Code Elimination (DCE)
class PassDeadCodeElimination final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "DeadCodeElimination"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 3. Copy Propagation
class PassCopyPropagation final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "CopyPropagation"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 4. Common Subexpression Elimination (CSE)
class PassCommonSubexpressionElimination final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "CommonSubexpressionElimination"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 5. Peephole Optimization
class PassPeepholeOptimization final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "PeepholeOptimization"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 6. Algebraic Simplification
class PassAlgebraicSimplification final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "AlgebraicSimplification"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 7. Branch Simplification
class PassBranchSimplification final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "BranchSimplification"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 8. SSA Cleanup
class PassSsaCleanup final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "SsaCleanup"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 9. Register Coalescing
class PassRegisterCoalescing final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "RegisterCoalescing"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

// 10. Instruction Scheduling
class PassInstructionScheduling final : public ShaderOptPass {
public:
	[[nodiscard]] const char* GetName() const noexcept override { return "InstructionScheduling"; }
	bool Run(ShaderIR& ir, PassStats& stats) override;
};

} // namespace Libs::Graphics::ShaderRecompiler::Opt

#endif // GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PASSES_H
