#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_H_

#include "common/common.h"
#include "common/stringUtils.h"
#include "graphics/shader/recompiler/ir/ResourceMaterialization.h"
#include "graphics/shader/shader.h"

#include <string>
#include <vector>

namespace Libs::Graphics::ShaderRecompiler::Msl {

bool ProgramRequiresExactSubgroupSize(const IR::Program& program);

bool EmitProgram(const IR::Program& program, const IR::ResourceSnapshot& resources,
                 const ShaderVertexInputInfo*  vertex_input_info,
                 const ShaderPixelInputInfo*   pixel_input_info,
                 const ShaderComputeInputInfo* compute_input_info, std::string& msl_source,
                 std::string* error);

} // namespace Libs::Graphics::ShaderRecompiler::Msl

#endif /* EMULATOR_INCLUDE_EMULATOR_GRAPHICS_SHADER_RECOMPILER_MSLEMITTER_H_ */
