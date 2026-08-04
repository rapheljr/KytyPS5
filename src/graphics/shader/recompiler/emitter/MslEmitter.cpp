#include "graphics/shader/recompiler/emitter/MslEmitter.h"
#include "graphics/shader/recompiler/emitter/mslEmitterInternal.h"

namespace Libs::Graphics::ShaderRecompiler::Msl {

bool ProgramRequiresExactSubgroupSize(const IR::Program& program) {
	return program.info.has_bitwise_xor;
}

bool EmitProgram(const IR::Program& program, const IR::ResourceSnapshot& resources,
                 const ShaderVertexInputInfo*  vertex_input_info,
                 const ShaderPixelInputInfo*   pixel_input_info,
                 const ShaderComputeInputInfo* compute_input_info, std::string& msl_source,
                 std::string* error) {
	Emitter::MslEmitterState state(program, resources);
	state.vertex_input_info  = vertex_input_info;
	state.pixel_input_info   = pixel_input_info;
	state.compute_input_info = compute_input_info;

	bool ok = Emitter::EmitModule(state, msl_source, error);
	if (!ok && error != nullptr && error->empty()) {
		*error = "MSL code emission failed";
	}
	return ok;
}

} // namespace Libs::Graphics::ShaderRecompiler::Msl
