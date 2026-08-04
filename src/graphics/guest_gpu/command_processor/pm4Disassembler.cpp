// pm4Disassembler.cpp
//
// PM4 packet disassembler, validation layer & command tracer for Phase K.

#include "graphics/guest_gpu/command_processor/pm4Disassembler.h"

#include <cstdio>
#include <sstream>

namespace Libs::Graphics::Pm4 {

std::string Pm4Disassembler::GetOpcodeName(uint32_t opcode) noexcept {
	switch (opcode) {
		case IT_NOP:                       return "IT_NOP";
		case IT_SET_BASE:                  return "IT_SET_BASE";
		case IT_CLEAR_STATE:               return "IT_CLEAR_STATE";
		case IT_INDEX_BUFFER_SIZE:         return "IT_INDEX_BUFFER_SIZE";
		case IT_DISPATCH_DIRECT:           return "IT_DISPATCH_DIRECT";
		case IT_DISPATCH_INDIRECT:         return "IT_DISPATCH_INDIRECT";
		case IT_SET_PREDICATION:           return "IT_SET_PREDICATION";
		case IT_COND_EXEC:                 return "IT_COND_EXEC";
		case IT_DRAW_INDIRECT:             return "IT_DRAW_INDIRECT";
		case IT_DRAW_INDEX_INDIRECT:       return "IT_DRAW_INDEX_INDIRECT";
		case IT_INDEX_BASE:                return "IT_INDEX_BASE";
		case IT_DRAW_INDEX_2:              return "IT_DRAW_INDEX_2";
		case IT_CONTEXT_CONTROL:           return "IT_CONTEXT_CONTROL";
		case IT_INDEX_TYPE:                return "IT_INDEX_TYPE";
		case IT_DRAW_INDEX_AUTO:           return "IT_DRAW_INDEX_AUTO";
		case IT_NUM_INSTANCES:             return "IT_NUM_INSTANCES";
		case IT_WRITE_DATA:                return "IT_WRITE_DATA";
		case IT_MEM_SEMAPHORE:             return "IT_MEM_SEMAPHORE";
		case IT_INDIRECT_BUFFER:           return "IT_INDIRECT_BUFFER";
		case IT_COPY_DATA:                 return "IT_COPY_DATA";
		case IT_CP_DMA:                    return "IT_CP_DMA";
		case IT_PFP_SYNC_ME:               return "IT_PFP_SYNC_ME";
		case IT_SURFACE_SYNC:              return "IT_SURFACE_SYNC";
		case IT_EVENT_WRITE:               return "IT_EVENT_WRITE";
		case IT_EVENT_WRITE_EOP:           return "IT_EVENT_WRITE_EOP";
		case IT_EVENT_WRITE_EOS:           return "IT_EVENT_WRITE_EOS";
		case IT_RELEASE_MEM:               return "IT_RELEASE_MEM";
		case IT_DMA_DATA:                  return "IT_DMA_DATA";
		case IT_ACQUIRE_MEM:               return "IT_ACQUIRE_MEM";
		case IT_SET_CONFIG_REG:            return "IT_SET_CONFIG_REG";
		case IT_SET_CONTEXT_REG:           return "IT_SET_CONTEXT_REG";
		case IT_SET_SH_REG:                return "IT_SET_SH_REG";
		case IT_SET_UCONFIG_REG:           return "IT_SET_UCONFIG_REG";
		default:                           return "IT_UNKNOWN";
	}
}

std::string Pm4Disassembler::DisassemblePacket(const DecodedPacket& packet) {
	std::stringstream ss;
	std::string op_name = GetOpcodeName(packet.opcode);
	char buf[256];

	std::snprintf(buf, sizeof(buf), "[PM4 Type3] Opcode: 0x%02X (%s), CountDW: %u",
	              packet.opcode, op_name.c_str(), packet.header.count_dw);
	ss << buf;

	std::visit([&ss](auto&& arg) {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, DrawPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | DRAW: Indexed=%d, VertexCount=%u, IndexCount=%u, Indirect=%d",
			              arg.indexed ? 1 : 0, arg.vertex_count, arg.index_count, arg.indirect ? 1 : 0);
			ss << dbuf;
		} else if constexpr (std::is_same_v<T, DispatchPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | DISPATCH: Groups=(%u, %u, %u), Indirect=%d",
			              arg.group_x, arg.group_y, arg.group_z, arg.indirect ? 1 : 0);
			ss << dbuf;
		} else if constexpr (std::is_same_v<T, DmaCopyPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | DMA: Src=0x%llX -> Dst=0x%llX, Bytes=%u",
			              static_cast<unsigned long long>(arg.src_addr),
			              static_cast<unsigned long long>(arg.dst_addr), arg.num_bytes);
			ss << dbuf;
		} else if constexpr (std::is_same_v<T, ClearPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | CLEAR: Color=(%.2f, %.2f, %.2f, %.2f), Depth=%.2f",
			              arg.color[0], arg.color[1], arg.color[2], arg.color[3], arg.depth);
			ss << dbuf;
		} else if constexpr (std::is_same_v<T, BarrierPacket>) {
			ss << " | BARRIER: FlushCB/DB=" << (arg.flush_inv_cb ? 1 : 0);
		} else if constexpr (std::is_same_v<T, EventPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | EVENT: Type=%u, WriteEOP=%d, Dst=0x%llX",
			              arg.event_type, arg.write_eop ? 1 : 0, static_cast<unsigned long long>(arg.dst_gpu_addr));
			ss << dbuf;
		} else if constexpr (std::is_same_v<T, SetRegisterPacket>) {
			char dbuf[128];
			std::snprintf(dbuf, sizeof(dbuf), " | SET_REG: Offset=0x%04X, Count=%u, ContextReg=%d",
			              arg.reg_offset, arg.count, arg.is_context_reg ? 1 : 0);
			ss << dbuf;
		}
	}, packet.data);

	return ss.str();
}

std::string Pm4Disassembler::DisassembleStream(const uint32_t* stream_ptr, size_t size_dw) {
	std::stringstream disasm;
	Pm4RingBufferParser parser;
	parser.SetPacketCallback([&disasm](const DecodedPacket& packet) {
		disasm << DisassemblePacket(packet) << "\n";
	});
	parser.ParseStream(stream_ptr, size_dw);
	return disasm.str();
}

std::vector<ValidationIssue> Pm4Disassembler::ValidateStream(const uint32_t* stream_ptr, size_t size_dw) noexcept {
	std::vector<ValidationIssue> issues;
	if (!stream_ptr || size_dw == 0) {
		return issues;
	}

	size_t offset = 0;
	while (offset < size_dw) {
		PacketHeader header = Pm4RingBufferParser::DecodeHeader(stream_ptr[offset]);
		if (header.type == PacketHeaderType::Invalid) {
			ValidationIssue issue{};
			issue.severity     = ValidationIssue::Severity::Error;
			issue.dword_offset = offset;
			issue.opcode       = 0;
			issue.message      = "Invalid PM4 packet header type";
			issues.push_back(issue);
			offset++;
			continue;
		}

		if (header.count_dw > (size_dw - offset)) {
			ValidationIssue issue{};
			issue.severity     = ValidationIssue::Severity::Critical;
			issue.dword_offset = offset;
			issue.opcode       = header.opcode;
			issue.message      = "PM4 packet length extends past ring buffer boundary";
			issues.push_back(issue);
			break;
		}

		offset += header.count_dw;
	}

	return issues;
}

} // namespace Libs::Graphics::Pm4
