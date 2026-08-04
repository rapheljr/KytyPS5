// pm4Parser.cpp
//
// Zero-copy PS5 PM4 ring buffer parser & packet decoder for Phase K.

#include "graphics/guest_gpu/command_processor/pm4Parser.h"

#include <cstring>

namespace Libs::Graphics::Pm4 {

PacketHeader Pm4RingBufferParser::DecodeHeader(uint32_t header_dw) noexcept {
	PacketHeader header{};
	header.raw = header_dw;

	uint32_t type_bits = (header_dw >> 30u) & 0x3u;
	if (type_bits == 3u) {
		header.type     = PacketHeaderType::Type3;
		header.opcode   = static_cast<uint8_t>((header_dw >> 8u) & 0xFFu);
		header.count_dw = static_cast<uint16_t>(((header_dw >> 16u) & 0x3FFFu) + 2u);
	} else if (type_bits == 0u) {
		header.type     = PacketHeaderType::Type0;
		header.opcode   = 0;
		header.count_dw = static_cast<uint16_t>(((header_dw >> 16u) & 0x3FFFu) + 2u);
	} else {
		header.type     = PacketHeaderType::Invalid;
		header.opcode   = 0;
		header.count_dw = 0;
	}

	return header;
}

bool Pm4RingBufferParser::ParseStream(const uint32_t* stream_ptr, size_t size_dw, uint32_t max_depth) {
	if (!stream_ptr || size_dw == 0) {
		return true;
	}

	size_t current_offset = 0;
	while (current_offset < size_dw) {
		size_t consumed_dw = 0;
		bool success = ParsePacketInternal(stream_ptr + current_offset, size_dw - current_offset, consumed_dw, 0, max_depth);
		if (!success) {
			m_stats.parse_errors++;
			if (m_error_callback) {
				uint32_t raw_h = stream_ptr[current_offset];
				ParserErrorAction action = m_error_callback(raw_h, current_offset, "Corrupt or truncated PM4 packet");
				if (action == ParserErrorAction::AbortStream) {
					return false;
				}
				if (consumed_dw == 0) {
					consumed_dw = 1; // Advance past bad header dword
				}
			} else {
				return false;
			}
		}

		if (consumed_dw == 0) {
			consumed_dw = 1;
		}
		current_offset += consumed_dw;
	}

	return true;
}

bool Pm4RingBufferParser::ParsePacketInternal(const uint32_t* ptr, size_t remaining_dw, size_t& out_consumed_dw, uint32_t current_depth, uint32_t max_depth) {
	out_consumed_dw = 0;
	if (remaining_dw == 0) {
		return false;
	}

	PacketHeader header = DecodeHeader(ptr[0]);
	if (header.type != PacketHeaderType::Type3 && header.type != PacketHeaderType::Type0) {
		return false;
	}

	if (header.count_dw > remaining_dw) {
		return false; // Packet extends past buffer boundary
	}

	out_consumed_dw = header.count_dw;
	m_stats.packets_parsed++;
	m_stats.dwords_parsed += header.count_dw;

	uint32_t payload_dw = (header.count_dw > 1) ? (header.count_dw - 1) : 0;
	const uint32_t* payload = (payload_dw > 0) ? (ptr + 1) : nullptr;

	// Handle Indirect Buffers recursively if opcode is IT_INDIRECT_BUFFER
	if (header.IsType3() && header.opcode == IT_INDIRECT_BUFFER && payload_dw >= 2) {
		m_stats.indirect_buffers++;
		if (current_depth < max_depth) {
			uint64_t ib_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			uint32_t ib_size_dw  = payload[2] & 0xFFFFF8u; // Dword size alignment mask
			const uint32_t* ib_ptr = reinterpret_cast<const uint32_t*>(ib_gpu_addr);

			if (ib_ptr && ib_size_dw > 0) {
				size_t ib_offset = 0;
				while (ib_offset < ib_size_dw) {
					size_t ib_consumed = 0;
					ParsePacketInternal(ib_ptr + ib_offset, ib_size_dw - ib_offset, ib_consumed, current_depth + 1, max_depth);
					if (ib_consumed == 0) ib_consumed = 1;
					ib_offset += ib_consumed;
				}
			}
		}
	}

	DecodedPacket decoded_pkt{};
	decoded_pkt.header     = header;
	decoded_pkt.opcode     = header.opcode;
	decoded_pkt.payload    = payload;
	decoded_pkt.payload_dw = payload_dw;
	decoded_pkt.data       = DecodePayload(header, payload, payload_dw);

	if (m_packet_callback) {
		m_packet_callback(decoded_pkt);
	}

	return true;
}

DecodedPacketData Pm4RingBufferParser::DecodePayload(const PacketHeader& header, const uint32_t* payload, uint32_t payload_dw) {
	if (!header.IsType3() || !payload || payload_dw == 0) {
		return SetRegisterPacket{};
	}

	switch (header.opcode) {
		case IT_DRAW_INDEX_2:
		case IT_DRAW_INDEX_AUTO:
		case IT_DRAW_INDIRECT:
		case IT_DRAW_INDEX_INDIRECT: {
			m_stats.draw_packets++;
			DrawPacket draw{};
			if (header.opcode == IT_DRAW_INDEX_2 && payload_dw >= 3) {
				draw.index_count    = payload[0];
				draw.vertex_count   = payload[0];
				draw.indexed        = true;
				draw.first_vertex   = payload[1];
			} else if (header.opcode == IT_DRAW_INDEX_AUTO && payload_dw >= 1) {
				draw.index_count    = payload[0];
				draw.vertex_count   = payload[0];
				draw.indexed        = false;
			} else if (header.opcode == IT_DRAW_INDIRECT && payload_dw >= 2) {
				draw.indirect       = true;
				draw.indexed        = false;
				draw.indirect_args  = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			} else if (header.opcode == IT_DRAW_INDEX_INDIRECT && payload_dw >= 2) {
				draw.indirect       = true;
				draw.indexed        = true;
				draw.indirect_args  = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			return draw;
		}

		case IT_DISPATCH_DIRECT:
		case IT_DISPATCH_INDIRECT: {
			m_stats.dispatch_packets++;
			DispatchPacket dispatch{};
			if (header.opcode == IT_DISPATCH_DIRECT && payload_dw >= 3) {
				dispatch.group_x  = payload[0];
				dispatch.group_y  = payload[1];
				dispatch.group_z  = payload[2];
				dispatch.indirect = false;
			} else if (header.opcode == IT_DISPATCH_INDIRECT && payload_dw >= 2) {
				dispatch.indirect      = true;
				dispatch.indirect_args = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			return dispatch;
		}

		case IT_CP_DMA:
		case IT_DMA_DATA: {
			m_stats.dma_packets++;
			DmaCopyPacket dma{};
			if (payload_dw >= 4) {
				dma.src_addr  = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
				dma.dst_addr  = (static_cast<uint64_t>(payload[3]) << 32u) | payload[2];
				dma.num_bytes = (payload_dw >= 5) ? payload[4] : 0;
			}
			return dma;
		}

		case IT_ACQUIRE_MEM:
		case IT_RELEASE_MEM:
		case IT_SURFACE_SYNC: {
			m_stats.barrier_packets++;
			BarrierPacket barrier{};
			barrier.flush_inv_cb = true;
			barrier.flush_inv_db = true;
			return barrier;
		}

		case IT_EVENT_WRITE:
		case IT_EVENT_WRITE_EOP:
		case IT_EVENT_WRITE_EOS: {
			EventPacket evt{};
			evt.event_type = payload[0] & 0xFFu;
			evt.write_eop  = (header.opcode == IT_EVENT_WRITE_EOP);
			if (payload_dw >= 3) {
				evt.dst_gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
			}
			if (payload_dw >= 4) {
				evt.value = payload[3];
			}
			return evt;
		}

		case IT_SET_CONTEXT_REG:
		case IT_SET_SH_REG:
		case IT_SET_CONFIG_REG:
		case IT_SET_UCONFIG_REG: {
			SetRegisterPacket reg{};
			reg.reg_offset     = payload[0] & 0xFFFFu;
			reg.count          = payload_dw - 1;
			reg.values         = payload + 1;
			reg.is_context_reg = (header.opcode == IT_SET_CONTEXT_REG);
			reg.is_sh_reg      = (header.opcode == IT_SET_SH_REG);
			return reg;
		}

		case IT_SET_BASE: {
			SetBasePacket base{};
			base.base_type = payload[0];
			if (payload_dw >= 3) {
				base.gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
			}
			return base;
		}

		case IT_CLEAR_STATE: {
			ClearStatePacket clr{};
			if (payload_dw >= 1) clr.flags = payload[0];
			return clr;
		}

		case IT_INDEX_BUFFER_SIZE:
		case IT_INDEX_BASE:
		case IT_INDEX_TYPE: {
			IndexBufferInfoPacket ib_info{};
			if (header.opcode == IT_INDEX_BUFFER_SIZE && payload_dw >= 1) {
				ib_info.size_bytes = payload[0] * 4;
			} else if (header.opcode == IT_INDEX_BASE && payload_dw >= 2) {
				ib_info.gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			} else if (header.opcode == IT_INDEX_TYPE && payload_dw >= 1) {
				ib_info.index_type = payload[0];
			}
			return ib_info;
		}

		case IT_SET_PREDICATION: {
			SetPredicationPacket pred{};
			if (payload_dw >= 2) {
				pred.query_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				pred.pred_enable = ((payload[2] & 0x1u) != 0);
				pred.hint_draw   = ((payload[2] & 0x2u) != 0);
			}
			return pred;
		}

		case IT_COND_EXEC: {
			CondExecPacket cond{};
			if (payload_dw >= 2) {
				cond.test_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				cond.skip_count_dw = payload[2];
			}
			return cond;
		}

		case IT_CONTEXT_CONTROL: {
			ContextControlPacket ctrl{};
			if (payload_dw >= 2) {
				ctrl.load_control   = payload[0];
				ctrl.shadow_control = payload[1];
			}
			return ctrl;
		}

		case IT_DRAW_INDIRECT_MULTI:
		case IT_DRAW_INDEX_INDIRECT_MULTI: {
			MultiDrawIndirectPacket multi{};
			multi.indexed = (header.opcode == IT_DRAW_INDEX_INDIRECT_MULTI);
			if (payload_dw >= 3) {
				multi.indirect_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
				multi.draw_count        = payload[2];
			}
			if (payload_dw >= 4) {
				multi.stride_bytes = payload[3];
			}
			return multi;
		}

		case IT_NUM_INSTANCES: {
			NumInstancesPacket num{};
			if (payload_dw >= 1) num.instance_count = payload[0];
			return num;
		}

		case IT_MEM_SEMAPHORE: {
			MemSemaphorePacket sem{};
			if (payload_dw >= 2) {
				sem.sem_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				sem.sem_op = payload[2];
			}
			return sem;
		}

		case IT_REWIND: {
			RewindPacket rwd{};
			if (payload_dw >= 1) rwd.rewind_offset = payload[0];
			return rwd;
		}

		default:
			return SetRegisterPacket{};
	}
}

} // namespace Libs::Graphics::Pm4
