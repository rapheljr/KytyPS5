#include "graphics/shader/recompiler/emitter/mslEmitterInternal.h"

#include <fmt/format.h>

namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter {

bool EmitMemoryOp(MslEmitterState& state, const IR::Instruction& inst) {
	std::string dest = (inst.dst.kind != IR::OperandKind::Null) ? state.FormatOperand(inst.dst) : "";
	std::string src0 = (inst.src_count > 0) ? state.FormatOperand(inst.src[0]) : "";
	std::string src1 = (inst.src_count > 1) ? state.FormatOperand(inst.src[1]) : "";
	std::string src2 = (inst.src_count > 2) ? state.FormatOperand(inst.src[2]) : "";
	std::string src3 = (inst.src_count > 3) ? state.FormatOperand(inst.src[3]) : "";

	switch (inst.op) {
		// Constant & Buffer Loads
		case IR::Opcode::SLoadDword:
		case IR::Opcode::SBufferLoadDword:
		case IR::Opcode::BufferLoadDword:
			state.body_ss << fmt::format("    {} = storage_buffers[({}) >> 2];\n", dest, src0);
			return true;

		case IR::Opcode::BufferLoadUbyte:
			state.body_ss << fmt::format("    {} = static_cast<uint>(reinterpret_cast<device uchar*>(storage_buffers)[{}]);\n", dest, src0);
			return true;

		case IR::Opcode::BufferLoadSbyte:
			state.body_ss << fmt::format("    {} = static_cast<uint>(static_cast<int>(reinterpret_cast<device char*>(storage_buffers)[{}]));\n", dest, src0);
			return true;

		case IR::Opcode::BufferLoadUshort:
			state.body_ss << fmt::format("    {} = static_cast<uint>(reinterpret_cast<device ushort*>(storage_buffers)[({}) >> 1]);\n", dest, src0);
			return true;

		case IR::Opcode::BufferLoadSshort:
			state.body_ss << fmt::format("    {} = static_cast<uint>(static_cast<int>(reinterpret_cast<device short*>(storage_buffers)[({}) >> 1]));\n", dest, src0);
			return true;

		// Buffer Stores
		case IR::Opcode::BufferStoreByte:
			state.body_ss << fmt::format("    reinterpret_cast<device uchar*>(storage_buffers)[{}] = static_cast<uchar>({});\n", src0, src1);
			return true;

		case IR::Opcode::BufferStoreShort:
			state.body_ss << fmt::format("    reinterpret_cast<device ushort*>(storage_buffers)[({}) >> 1] = static_cast<ushort>({});\n", src0, src1);
			return true;

		case IR::Opcode::BufferStoreDword:
			state.body_ss << fmt::format("    storage_buffers[({}) >> 2] = {};\n", src0, src1);
			return true;

		// LDS / Threadgroup Memory
		case IR::Opcode::DsReadB32:
			state.body_ss << fmt::format("    {} = lds[({}) >> 2];\n", dest, src0);
			return true;

		case IR::Opcode::DsWriteB32:
			state.body_ss << fmt::format("    lds[({}) >> 2] = {};\n", src0, src1);
			return true;

		default:
			return false;
	}
}

bool EmitExportOp(MslEmitterState& state, const IR::Instruction& inst) {
	if (inst.op != IR::Opcode::Export) {
		return false;
	}

	std::string src0 = (inst.src_count > 0) ? state.FormatOperand(inst.src[0]) : "0u";
	std::string src1 = (inst.src_count > 1) ? state.FormatOperand(inst.src[1]) : "0u";
	std::string src2 = (inst.src_count > 2) ? state.FormatOperand(inst.src[2]) : "0u";
	std::string src3 = (inst.src_count > 3) ? state.FormatOperand(inst.src[3]) : "0u";

	switch (inst.export_info.kind) {
		case IR::ExportTargetKind::Position:
			if (state.stage == ShaderType::Vertex) {
				state.body_ss << fmt::format("    out.position = float4(as_type<float>({}), as_type<float>({}), as_type<float>({}), as_type<float>({}));\n",
				                             src0, src1, src2, src3);
			}
			return true;

		case IR::ExportTargetKind::Parameter:
			if (state.stage == ShaderType::Vertex) {
				uint32_t param_idx = inst.export_info.target;
				state.body_ss << fmt::format("    out.param{} = float4(as_type<float>({}), as_type<float>({}), as_type<float>({}), as_type<float>({}));\n",
				                             param_idx, src0, src1, src2, src3);
			}
			return true;

		case IR::ExportTargetKind::Mrt:
			if (state.stage == ShaderType::Pixel) {
				uint32_t mrt_idx = inst.export_info.target;
				state.body_ss << fmt::format("    out.color{} = float4(as_type<float>({}), as_type<float>({}), as_type<float>({}), as_type<float>({}));\n",
				                             mrt_idx, src0, src1, src2, src3);
			}
			return true;

		case IR::ExportTargetKind::MrtZ:
			if (state.stage == ShaderType::Pixel) {
				state.body_ss << fmt::format("    out.depth = as_type<float>({});\n", src0);
			}
			return true;

		default:
			return true;
	}
}

} // namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter
