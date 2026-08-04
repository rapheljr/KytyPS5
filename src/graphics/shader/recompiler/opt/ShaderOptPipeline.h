// ShaderOptPipeline.h
//
// Shader Optimization Pipeline Infrastructure for Phase L.

#ifndef GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PIPELINE_H
#define GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PIPELINE_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler::Opt {

enum class IROpcode : uint16_t {
	Nop = 0,
	Mov,
	FMov,
	Add,
	Sub,
	Mul,
	FAdd,
	FMul,
	And,
	Or,
	Xor,
	Branch,
	LoadStorage,
	LoadUniform
};

struct IROperand {
	bool     is_imm  = false;
	uint32_t reg_id  = 0;
	union {
		uint32_t imm_u32;
		float    imm_f32;
	};

	bool operator==(const IROperand& other) const {
		if (is_imm != other.is_imm) return false;
		if (is_imm) return imm_u32 == other.imm_u32;
		return reg_id == other.reg_id;
	}
};

struct IRInstruction {
	IROpcode   opcode            = IROpcode::Nop;
	uint32_t   dst_reg_id        = 0;
	IROperand  src0{};
	IROperand  src1{};
	IROperand  src2{};
	uint32_t   target_label_id   = 0;
	bool       active            = true;
	bool       is_side_effecting = false;
};

class ShaderIR {
public:
	ShaderIR() = default;
	~ShaderIR() = default;

	void AddInstruction(const IRInstruction& inst) {
		m_instructions.push_back(inst);
		if (inst.dst_reg_id > m_max_reg_id) m_max_reg_id = inst.dst_reg_id;
		if (!inst.src0.is_imm && inst.src0.reg_id > m_max_reg_id) m_max_reg_id = inst.src0.reg_id;
		if (!inst.src1.is_imm && inst.src1.reg_id > m_max_reg_id) m_max_reg_id = inst.src1.reg_id;
		if (!inst.src2.is_imm && inst.src2.reg_id > m_max_reg_id) m_max_reg_id = inst.src2.reg_id;
	}

	[[nodiscard]] std::vector<IRInstruction>& GetInstructions() noexcept { return m_instructions; }
	[[nodiscard]] const std::vector<IRInstruction>& GetInstructions() const noexcept { return m_instructions; }

	[[nodiscard]] uint32_t GetInstructionCount() const noexcept {
		uint32_t count = 0;
		for (const auto& inst : m_instructions) {
			if (inst.active) count++;
		}
		return count;
	}

	[[nodiscard]] uint32_t GetMaxRegisterId() const noexcept { return m_max_reg_id; }
	void SetMaxRegisterId(uint32_t max_reg_id) noexcept { m_max_reg_id = max_reg_id; }

	[[nodiscard]] uint32_t GetBasicBlockCount() const noexcept { return m_basic_block_count; }
	void SetBasicBlockCount(uint32_t count) noexcept { m_basic_block_count = count; }

private:
	std::vector<IRInstruction> m_instructions;
	uint32_t                   m_max_reg_id        = 0;
	uint32_t                   m_basic_block_count = 1;
};

enum class ShaderOptLevel : uint8_t {
	O0 = 0, // Disabled
	O1 = 1, // Basic (Constant folding & DCE)
	O2 = 2, // Full (Recommended: CSE, Algebraic, Copy Prop, Peephole)
	O3 = 3  // Aggressive (Scheduling & Register Coalescing)
};

struct PassStats {
	std::string pass_name;
	uint32_t    instructions_eliminated = 0;
	uint32_t    constants_folded        = 0;
	uint32_t    copies_propagated       = 0;
	uint32_t    registers_coalesced     = 0;
	double      execution_time_us       = 0.0;
};

struct PipelineStats {
	uint32_t               initial_instruction_count = 0;
	uint32_t               final_instruction_count   = 0;
	uint32_t               passes_run                = 0;
	double                 total_time_us             = 0.0;
	std::vector<PassStats> pass_stats;

	[[nodiscard]] double GetInstructionReductionPercent() const noexcept {
		if (initial_instruction_count == 0) return 0.0;
		return 100.0 * (1.0 - (static_cast<double>(final_instruction_count) / initial_instruction_count));
	}
};

class ShaderOptPass {
public:
	virtual ~ShaderOptPass() = default;
	[[nodiscard]] virtual const char* GetName() const noexcept = 0;
	virtual bool Run(ShaderIR& ir, PassStats& stats) = 0;
};

class ShaderOptPassManager {
public:
	explicit ShaderOptPassManager(ShaderOptLevel level = ShaderOptLevel::O2);
	~ShaderOptPassManager() = default;

	KYTY_CLASS_NO_COPY(ShaderOptPassManager);

	void SetOptLevel(ShaderOptLevel level) { m_level = level; }
	[[nodiscard]] ShaderOptLevel GetOptLevel() const noexcept { return m_level; }

	void SetDebugValidation(bool enable) noexcept { m_debug_validation = enable; }
	[[nodiscard]] bool GetDebugValidation() const noexcept { return m_debug_validation; }

	void AddPass(std::unique_ptr<ShaderOptPass> pass);
	void ConfigureForLevel(ShaderOptLevel level);

	bool Run(ShaderIR& ir);

	[[nodiscard]] const PipelineStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = PipelineStats{}; }

	[[nodiscard]] static bool ValidateIR(const ShaderIR& ir, std::string* out_error = nullptr);

private:
	ShaderOptLevel                              m_level = ShaderOptLevel::O2;
	bool                                        m_debug_validation = false;
	std::vector<std::unique_ptr<ShaderOptPass>> m_passes;
	PipelineStats                               m_stats{};
};

} // namespace Libs::Graphics::ShaderRecompiler::Opt

#endif // GRAPHICS_SHADER_RECOMPILER_OPT_SHADER_OPT_PIPELINE_H
