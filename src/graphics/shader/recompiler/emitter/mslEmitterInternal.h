#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_INTERNAL_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_INTERNAL_H_

#include "common/common.h"
#include "common/stringUtils.h"
#include "graphics/shader/recompiler/BufferFormat.h"
#include "graphics/shader/recompiler/ir/BindingLayout.h"
#include "graphics/shader/recompiler/ir/ResourceMaterialization.h"
#include "graphics/shader/recompiler/ir/ShaderIR.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter {

struct MslEmitterState {
	MslEmitterState(const IR::Program& program_, const IR::ResourceSnapshot& resources_)
	    : program(program_), resources(resources_) {}

	const IR::Program&                               program;
	const IR::ResourceSnapshot&                      resources;
	const ShaderVertexInputInfo*                     vertex_input_info  = nullptr;
	const ShaderPixelInputInfo*                      pixel_input_info   = nullptr;
	const ShaderComputeInputInfo*                    compute_input_info = nullptr;
	ShaderType                                       stage              = ShaderType::Unknown;
	uint32_t                                         wave_size          = 64;

	std::ostringstream                               headers_ss;
	std::ostringstream                               structs_ss;
	std::ostringstream                               helpers_ss;
	std::ostringstream                               body_ss;

	std::vector<std::string>                         unsupported_instructions;
	bool                                             has_unsupported_ops = false;

	// Helper register formatting
	std::string FormatOperand(const IR::Operand& op);
	std::string FormatRegister(const IR::Register& reg);
};

// Module emission functions
bool EmitModule(MslEmitterState& state, std::string& msl_source, std::string* error);

// Category emission handlers
bool EmitAluOp(MslEmitterState& state, const IR::Instruction& inst);
bool EmitControlFlowOp(MslEmitterState& state, const IR::Instruction& inst);
bool EmitImageOp(MslEmitterState& state, const IR::Instruction& inst);
bool EmitMemoryOp(MslEmitterState& state, const IR::Instruction& inst);
bool EmitExportOp(MslEmitterState& state, const IR::Instruction& inst);

} // namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_INTERNAL_H_ */
