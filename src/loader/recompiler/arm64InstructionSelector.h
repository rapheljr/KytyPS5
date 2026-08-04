// arm64InstructionSelector.h
//
// Pattern-Matching ARM64 Instruction Selector & Optimizer Engine.

#ifndef LOADER_RECOMPILER_ARM64_INSTRUCTION_SELECTOR_H
#define LOADER_RECOMPILER_ARM64_INSTRUCTION_SELECTOR_H

#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/compilerIR.h"

namespace Loader::Recompiler {

class Arm64InstructionSelector {
public:
	Arm64InstructionSelector() = default;
	~Arm64InstructionSelector() = default;

	KYTY_CLASS_NO_COPY(Arm64InstructionSelector);

	static bool SelectInstruction(const IRInstruction& inst, Arm64Emitter& emitter, const std::function<Arm64Reg(const VirtualReg&)>& vreg_map);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_INSTRUCTION_SELECTOR_H
