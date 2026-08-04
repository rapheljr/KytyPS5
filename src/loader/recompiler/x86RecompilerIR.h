// x86RecompilerIR.h
//
// Intermediate Representation, Basic Block & CFG for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_RECOMPILER_IR_H
#define LOADER_RECOMPILER_X86_RECOMPILER_IR_H

#include "common/common.h"
#include "loader/recompiler/x86Decoder.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace Loader::Recompiler {

struct RecompilerIRInstruction {
	X86Opcode    opcode      = X86Opcode::Nop;
	X86Condition cond        = X86Condition::Equal;
	bool         cond_invert = false;
	X86Operand   dst;
	X86Operand   src;
	uint64_t     guest_rip   = 0;
	bool         active      = true;
};

class RecompilerBasicBlock {
public:
	RecompilerBasicBlock(uint64_t start_rip = 0) : m_start_rip(start_rip) {}
	~RecompilerBasicBlock() = default;

	void AddInstruction(const RecompilerIRInstruction& inst) {
		m_instructions.push_back(inst);
	}

	[[nodiscard]] uint64_t GetStartRip() const noexcept { return m_start_rip; }
	[[nodiscard]] uint64_t GetEndRip() const noexcept { return m_end_rip; }
	void SetEndRip(uint64_t rip) noexcept { m_end_rip = rip; }

	[[nodiscard]] std::vector<RecompilerIRInstruction>& GetInstructions() noexcept { return m_instructions; }
	[[nodiscard]] const std::vector<RecompilerIRInstruction>& GetInstructions() const noexcept { return m_instructions; }

	[[nodiscard]] uint32_t GetActiveInstructionCount() const noexcept {
		uint32_t count = 0;
		for (const auto& inst : m_instructions) {
			if (inst.active) count++;
		}
		return count;
	}

private:
	uint64_t                        m_start_rip = 0;
	uint64_t                        m_end_rip   = 0;
	std::vector<RecompilerIRInstruction> m_instructions;
};

class X86BlockBuilder {
public:
	X86BlockBuilder() = default;
	~X86BlockBuilder() = default;

	KYTY_CLASS_NO_COPY(X86BlockBuilder);

	static std::unique_ptr<RecompilerBasicBlock> BuildBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t start_rip);
	static bool OptimizeBlock(RecompilerBasicBlock& block);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_RECOMPILER_IR_H
