#include "graphics/shader/recompiler/emitter/mslEmitterInternal.h"

#include <fmt/format.h>

namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter {

bool EmitControlFlowOp(MslEmitterState& state, const IR::Instruction& inst) {
	std::string dest = (inst.dst.kind != IR::OperandKind::Null) ? state.FormatOperand(inst.dst) : "";
	std::string src0 = (inst.src_count > 0) ? state.FormatOperand(inst.src[0]) : "";
	std::string src1 = (inst.src_count > 1) ? state.FormatOperand(inst.src[1]) : "";

	switch (inst.op) {
		case IR::Opcode::ControlNop:
		case IR::Opcode::Waitcnt:
		case IR::Opcode::InstPrefetch:
		case IR::Opcode::Sendmsg:
		case IR::Opcode::TtraceData:
			state.body_ss << "    /* nop */\n";
			return true;

		case IR::Opcode::Barrier:
			state.body_ss << "    threadgroup_barrier(mem_flags::mem_threadgroup);\n";
			return true;

		case IR::Opcode::LoadInputF32:
			if (state.stage == ShaderType::Vertex) {
				state.body_ss << fmt::format("    {} = as_type<uint>(in.attr0.x);\n", dest);
			} else if (state.stage == ShaderType::Pixel) {
				state.body_ss << fmt::format("    {} = as_type<uint>(in.position.x);\n", dest);
			} else {
				state.body_ss << fmt::format("    {} = 0u;\n", dest);
			}
			return true;

		case IR::Opcode::ReadFirstLaneU32:
			state.body_ss << fmt::format("    {} = simd_broadcast({}, 0);\n", dest, src0);
			return true;

		case IR::Opcode::ReadLaneU32:
			state.body_ss << fmt::format("    {} = simd_broadcast({}, {});\n", dest, src0, src1);
			return true;

		case IR::Opcode::WriteLaneU32:
			state.body_ss << fmt::format("    {} = {}; // write_lane\n", dest, src0);
			return true;

		default:
			return false;
	}
}

} // namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter
