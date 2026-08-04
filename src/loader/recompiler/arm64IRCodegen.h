// arm64IRCodegen.h
//
// Target-Independent Compiler IR to Native ARM64 Code Generator.

#ifndef LOADER_RECOMPILER_ARM64_IR_CODEGEN_H
#define LOADER_RECOMPILER_ARM64_IR_CODEGEN_H

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/arm64LinearScanAllocator.h"
#include "loader/recompiler/compilerIR.h"

namespace Loader::Recompiler {

class Arm64IRCodegen {
public:
	Arm64IRCodegen() = default;
	~Arm64IRCodegen() = default;

	KYTY_CLASS_NO_COPY(Arm64IRCodegen);

	bool CompileCFG(ControlFlowGraph& cfg, Arm64Emitter& emitter);

	[[nodiscard]] const LinearScanAllocator& GetAllocator() const noexcept { return m_allocator; }

private:
	Arm64Reg MapOperandToArm64Reg(const VirtualReg& vreg) const noexcept;

	LinearScanAllocator m_allocator;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_IR_CODEGEN_H
