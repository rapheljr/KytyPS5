#include "graphics/shader/recompiler/emitter/mslEmitterInternal.h"

#include <fmt/format.h>

namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter {

bool EmitAluOp(MslEmitterState& state, const IR::Instruction& inst) {
	std::string dest = (inst.dst.kind != IR::OperandKind::Null) ? state.FormatOperand(inst.dst) : "";
	std::string src0 = (inst.src_count > 0) ? state.FormatOperand(inst.src[0]) : "";
	std::string src1 = (inst.src_count > 1) ? state.FormatOperand(inst.src[1]) : "";
	std::string src2 = (inst.src_count > 2) ? state.FormatOperand(inst.src[2]) : "";

	switch (inst.op) {
		// Moves & Bitcasts
		case IR::Opcode::MoveU32:
		case IR::Opcode::MoveF32Bits:
			state.body_ss << fmt::format("    {} = {};\n", dest, src0);
			return true;

		case IR::Opcode::ConvertF32ToU32:
			state.body_ss << fmt::format("    {} = static_cast<uint>(as_type<float>({}));\n", dest, src0);
			return true;

		case IR::Opcode::ConvertU32ToF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(static_cast<float>({}));\n", dest, src0);
			return true;

		case IR::Opcode::ConvertF32ToI32:
			state.body_ss << fmt::format("    {} = as_type<uint>(static_cast<int>(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::ConvertI32ToF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(static_cast<float>(static_cast<int>({})));\n", dest, src0);
			return true;

		// Integer Arithmetic
		case IR::Opcode::IAddU32:
			state.body_ss << fmt::format("    {} = {} + {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::ISubU32:
			state.body_ss << fmt::format("    {} = {} - {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::IMulU32:
			state.body_ss << fmt::format("    {} = {} * {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::IMadI24U32:
		case IR::Opcode::UMadU24U32:
			state.body_ss << fmt::format("    {} = mad24({}, {}, {});\n", dest, src0, src1, src2);
			return true;

		case IR::Opcode::AbsI32:
			state.body_ss << fmt::format("    {} = static_cast<uint>(abs(static_cast<int>({})));\n", dest, src0);
			return true;

		case IR::Opcode::IMinI32:
			state.body_ss << fmt::format("    {} = static_cast<uint>(min(static_cast<int>({}), static_cast<int>({})));\n", dest, src0, src1);
			return true;

		case IR::Opcode::IMaxI32:
			state.body_ss << fmt::format("    {} = static_cast<uint>(max(static_cast<int>({}), static_cast<int>({})));\n", dest, src0, src1);
			return true;

		case IR::Opcode::UMinU32:
			state.body_ss << fmt::format("    {} = min({}, {});\n", dest, src0, src1);
			return true;

		case IR::Opcode::UMaxU32:
			state.body_ss << fmt::format("    {} = max({}, {});\n", dest, src0, src1);
			return true;

		// Floating-Point Arithmetic
		case IR::Opcode::FAddF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(as_type<float>({}) + as_type<float>({}));\n", dest, src0, src1);
			return true;

		case IR::Opcode::FSubF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(as_type<float>({}) - as_type<float>({}));\n", dest, src0, src1);
			return true;

		case IR::Opcode::FMulF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(as_type<float>({}) * as_type<float>({}));\n", dest, src0, src1);
			return true;

		case IR::Opcode::FMadF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(fma(as_type<float>({}), as_type<float>({}), as_type<float>({})));\n", dest, src0, src1, src2);
			return true;

		case IR::Opcode::FMinF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(fmin(as_type<float>({}), as_type<float>({})));\n", dest, src0, src1);
			return true;

		case IR::Opcode::FMaxF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(fmax(as_type<float>({}), as_type<float>({})));\n", dest, src0, src1);
			return true;

		case IR::Opcode::SqrtF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(sqrt(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::InverseSqrtF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(rsqrt(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::RcpF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(1.0f / as_type<float>({}));\n", dest, src0);
			return true;

		case IR::Opcode::Exp2F32:
			state.body_ss << fmt::format("    {} = as_type<uint>(exp2(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::Log2F32:
			state.body_ss << fmt::format("    {} = as_type<uint>(log2(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::SinF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(sin(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::CosF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(cos(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::FloorF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(floor(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::CeilF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(ceil(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::TruncF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(trunc(as_type<float>({})));\n", dest, src0);
			return true;

		case IR::Opcode::RoundEvenF32:
			state.body_ss << fmt::format("    {} = as_type<uint>(rint(as_type<float>({})));\n", dest, src0);
			return true;

		// Bitwise & Shift
		case IR::Opcode::BitwiseAndU32:
			state.body_ss << fmt::format("    {} = {} & {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::BitwiseOrU32:
			state.body_ss << fmt::format("    {} = {} | {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::BitwiseXorU32:
			state.body_ss << fmt::format("    {} = {} ^ {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::BitwiseNotU32:
			state.body_ss << fmt::format("    {} = ~{};\n", dest, src0);
			return true;

		case IR::Opcode::ShiftLeftLogicalU32:
			state.body_ss << fmt::format("    {} = {} << {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::ShiftRightLogicalU32:
			state.body_ss << fmt::format("    {} = {} >> {};\n", dest, src0, src1);
			return true;

		case IR::Opcode::ShiftRightArithmeticI32:
			state.body_ss << fmt::format("    {} = static_cast<uint>(static_cast<int>({}) >> {});\n", dest, src0, src1);
			return true;

		case IR::Opcode::BitCountU32:
			state.body_ss << fmt::format("    {} = popcount({});\n", dest, src0);
			return true;

		case IR::Opcode::BitReverseU32:
			state.body_ss << fmt::format("    {} = reverse_bits({});\n", dest, src0);
			return true;

		case IR::Opcode::FindLsbU32:
			state.body_ss << fmt::format("    {} = ctz({});\n", dest, src0);
			return true;

		case IR::Opcode::FindMsbFromHighU32:
			state.body_ss << fmt::format("    {} = 31u - clz({});\n", dest, src0);
			return true;

		// Select
		case IR::Opcode::SelectU32:
			state.body_ss << fmt::format("    {} = ({}) ? {} : {};\n", dest, src0, src1, src2);
			return true;

		default:
			return false;
	}
}

} // namespace Libs::Graphics::ShaderRecompiler::Msl::Emitter
