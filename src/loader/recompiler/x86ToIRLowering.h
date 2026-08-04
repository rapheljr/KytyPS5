// x86ToIRLowering.h
//
// Front-End x86-64 to Target-Independent Compiler IR Lowering Engine.

#ifndef LOADER_RECOMPILER_X86_TO_IR_LOWERING_H
#define LOADER_RECOMPILER_X86_TO_IR_LOWERING_H

#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/x86Decoder.h"

#include <memory>

namespace Loader::Recompiler {

class X86ToIRLowering {
public:
	X86ToIRLowering() = default;
	~X86ToIRLowering() = default;

	KYTY_CLASS_NO_COPY(X86ToIRLowering);

	static std::unique_ptr<ControlFlowGraph> LowerBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t start_rip);

private:
	static Value MapX86OperandToIR(ControlFlowGraph& cfg, BasicBlock* bb, const X86Operand& op);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_TO_IR_LOWERING_H
