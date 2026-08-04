#include "graphics/shader/recompiler/emitter/mslEmitterInternal.h"

#include <fmt/format.h>

namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter {

bool EmitImageOp(MslEmitterState& state, const IR::Instruction& inst) {
	std::string dest = (inst.dst.kind != IR::OperandKind::Null) ? state.FormatOperand(inst.dst) : "";
	std::string src0 = (inst.src_count > 0) ? state.FormatOperand(inst.src[0]) : "";
	std::string src1 = (inst.src_count > 1) ? state.FormatOperand(inst.src[1]) : "";
	std::string src2 = (inst.src_count > 2) ? state.FormatOperand(inst.src[2]) : "";

	switch (inst.op) {
		case IR::Opcode::ImageSample:
		case IR::Opcode::ImageGather4:
		case IR::Opcode::ImageGetLod:
			state.body_ss << fmt::format("    {} = as_type<uint>(1.0f); // ImageSample stub\n", dest);
			return true;

		case IR::Opcode::ImageLoad:
			state.body_ss << fmt::format("    {} = 0u; // ImageLoad stub\n", dest);
			return true;

		case IR::Opcode::ImageStore:
			state.body_ss << "    /* ImageStore stub */\n";
			return true;

		case IR::Opcode::ImageGetResinfo:
			state.body_ss << fmt::format("    {} = 512u; // ImageGetResinfo stub\n", dest);
			return true;

		default:
			return false;
	}
}

} // namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter
