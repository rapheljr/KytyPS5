// pm4Parser.cpp
//
// Zero-copy PS5 PM4 ring buffer parser & packet decoder.
// Implements decoding for 100% of AMD GCN/RDNA PM4 Type-3 opcodes.

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

bool Pm4RingBufferParser::ParseDrawIndexAuto(const uint32_t* payload, uint32_t payload_dw, DrawPacket& out_draw) {
	if (!payload || payload_dw < 1) {
		return false;
	}
	out_draw.vertex_count   = payload[0];
	out_draw.index_count    = payload[0];
	out_draw.indexed        = false;
	out_draw.first_vertex   = 0;
	out_draw.instance_count = 1;
	return true;
}

bool Pm4RingBufferParser::ParseDrawIndex(const uint32_t* payload, uint32_t payload_dw, DrawPacket& out_draw) {
	if (!payload || payload_dw < 3) {
		return false;
	}
	out_draw.index_count    = payload[0];
	out_draw.vertex_count   = payload[0];
	out_draw.index_gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
	out_draw.indexed        = true;
	out_draw.instance_count = 1;
	return true;
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

	// Handle Indirect Buffers recursively
	if (header.IsType3() &&
	    (header.opcode == IT_INDIRECT_BUFFER || header.opcode == IT_INDIRECT_BUFFER_CNST) &&
	    payload_dw >= 3) {
		m_stats.indirect_buffers++;
		if (current_depth < max_depth) {
			uint64_t ib_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | (payload[0] & ~3u);
			uint32_t ib_size_dw  = payload[2] & 0xFFFFF8u;
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
	// For Type0 (register write) or empty packets, return a generic register packet
	if (!header.IsType3()) {
		return SetRegisterPacket{};
	}

	// Handle zero-payload NOP case
	if (!payload || payload_dw == 0) {
		if (header.opcode == IT_NOP) {
			m_stats.nop_packets++;
			return NopPacket{nullptr, 0};
		}
		return SetRegisterPacket{};
	}

	switch (header.opcode) {

		// ─── NOP ──────────────────────────────────────────────────────────────
		case IT_NOP: {
			m_stats.nop_packets++;
			NopPacket nop{};
			nop.payload    = payload;
			nop.payload_dw = payload_dw;
			return nop;
		}

		// ─── SET_BASE: GDS / IB / CE base address setup ───────────────────────
		case IT_SET_BASE: {
			SetBasePacket base{};
			base.base_type = payload[0] & 0xFu;
			if (payload_dw >= 3) {
				base.gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | (payload[1] & ~3u);
			}
			return base;
		}

		// ─── CLEAR_STATE: Reset context registers to default ──────────────────
		case IT_CLEAR_STATE: {
			ClearStatePacket clr{};
			if (payload_dw >= 1) clr.flags = payload[0];
			return clr;
		}

		// ─── INDEX_BASE / INDEX_BUFFER_SIZE / INDEX_TYPE ─────────────────────
		case IT_INDEX_BUFFER_SIZE:
		case IT_INDEX_BASE:
		case IT_INDEX_TYPE: {
			IndexBufferInfoPacket ib_info{};
			if (header.opcode == IT_INDEX_BUFFER_SIZE && payload_dw >= 1) {
				ib_info.size_bytes = payload[0] * 4u; // size in dwords → bytes
			} else if (header.opcode == IT_INDEX_BASE && payload_dw >= 2) {
				ib_info.gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | (payload[0] & ~3u);
			} else if (header.opcode == IT_INDEX_TYPE && payload_dw >= 1) {
				ib_info.index_type = payload[0] & 0x3u; // VGT_INDEX_TYPE[1:0]
			}
			return ib_info;
		}

		// ─── DISPATCH_DIRECT / DISPATCH_INDIRECT ──────────────────────────────
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

		// ─── SET_PREDICATION: Conditional rendering ───────────────────────────
		case IT_SET_PREDICATION: {
			SetPredicationPacket pred{};
			if (payload_dw >= 2) {
				pred.query_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				uint32_t ctrl     = payload[2];
				pred.pred_enable  = ((ctrl & 0x1u) != 0);
				pred.pred_op      = (ctrl >> 16u) & 0x7u;
				pred.hint_draw    = ((ctrl & 0x100u) != 0);
				pred.continue_bit = ((ctrl & 0x200u) != 0);
			}
			return pred;
		}

		// ─── COND_EXEC: Conditional packet skip ───────────────────────────────
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

		// ─── DRAW packets ─────────────────────────────────────────────────────
		case IT_DRAW_INDEX_2:
		case IT_DRAW_INDEX_OFFSET_2:
		case IT_DRAW_INDEX_AUTO:
		case IT_DRAW_INDIRECT:
		case IT_DRAW_INDEX_INDIRECT: {
			m_stats.draw_packets++;
			DrawPacket draw{};
			if (header.opcode == IT_DRAW_INDEX_2 && payload_dw >= 3) {
				draw.index_count    = payload[0];
				draw.vertex_count   = payload[0];
				draw.index_gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
				draw.indexed        = true;
			} else if (header.opcode == IT_DRAW_INDEX_OFFSET_2 && payload_dw >= 4) {
				draw.index_count    = payload[1];
				draw.vertex_count   = payload[1];
				draw.index_offset   = payload[0]; // base index offset
				draw.index_gpu_addr = (static_cast<uint64_t>(payload[3]) << 32u) | payload[2];
				draw.indexed        = true;
			} else if (header.opcode == IT_DRAW_INDEX_AUTO && payload_dw >= 1) {
				draw.vertex_count   = payload[0];
				draw.index_count    = payload[0];
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

		// ─── CONTEXT_CONTROL: Context save/restore ────────────────────────────
		case IT_CONTEXT_CONTROL: {
			ContextControlPacket ctrl{};
			if (payload_dw >= 1) ctrl.load_control   = payload[0];
			if (payload_dw >= 2) ctrl.shadow_control = payload[1];
			return ctrl;
		}

		// ─── DRAW_INDIRECT_MULTI / DRAW_INDEX_INDIRECT_MULTI ─────────────────
		case IT_DRAW_INDIRECT_MULTI:
		case IT_DRAW_INDEX_INDIRECT_MULTI: {
			m_stats.draw_packets++;
			MultiDrawIndirectPacket multi{};
			multi.indexed = (header.opcode == IT_DRAW_INDEX_INDIRECT_MULTI);
			if (payload_dw >= 2) {
				multi.indirect_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				// Word[2]: bit31=count_indirect, bits[30:0]=draw_count
				uint32_t w2 = payload[2];
				multi.count_indirect = ((w2 >> 31u) & 1u) != 0;
				multi.draw_count     = w2 & 0x7FFFFFFFu;
			}
			if (payload_dw >= 5 && multi.count_indirect) {
				// count_gpu_addr in words 3 & 4 when count_indirect=1
				multi.count_gpu_addr = (static_cast<uint64_t>(payload[4]) << 32u) | payload[3];
			}
			if (payload_dw >= 6) {
				multi.stride_bytes = payload[5];
			} else if (payload_dw >= 4 && !multi.count_indirect) {
				multi.stride_bytes = payload[3];
			}
			return multi;
		}

		// ─── NUM_INSTANCES ────────────────────────────────────────────────────
		case IT_NUM_INSTANCES: {
			NumInstancesPacket num{};
			if (payload_dw >= 1) num.instance_count = payload[0];
			return num;
		}

		// ─── INDIRECT_BUFFER / INDIRECT_BUFFER_CNST ───────────────────────────
		case IT_INDIRECT_BUFFER:
		case IT_INDIRECT_BUFFER_CNST: {
			IndirectBufferPacket ib{};
			ib.chain_mode = (header.opcode == IT_INDIRECT_BUFFER_CNST);
			if (payload_dw >= 2) {
				ib.ib_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | (payload[0] & ~3u);
			}
			if (payload_dw >= 3) {
				ib.ib_size_dw  = payload[2] & 0xFFFFFu;
				ib.vmid        = (payload[2] >> 24u) & 0xFu;
				ib.preempt_en  = ((payload[2] >> 28u) & 1u) != 0;
			}
			return ib;
		}

		// ─── WRITE_DATA ───────────────────────────────────────────────────────
		case IT_WRITE_DATA: {
			m_stats.write_data_packets++;
			WriteDataPacket wd{};
			if (payload_dw >= 1) {
				uint32_t ctrl   = payload[0];
				wd.dst_sel      = (ctrl >> 8u)  & 0xFu;
				wd.wr_confirm   = (ctrl >> 20u) & 0x1u;
				wd.engine_sel   = (ctrl >> 30u) & 0x3u;
			}
			if (payload_dw >= 3) {
				wd.dst_gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
			}
			if (payload_dw > 3) {
				wd.data     = payload + 3;
				wd.count_dw = payload_dw - 3u;
			}
			return wd;
		}

		// ─── MEM_SEMAPHORE ────────────────────────────────────────────────────
		case IT_MEM_SEMAPHORE: {
			MemSemaphorePacket sem{};
			if (payload_dw >= 2) {
				sem.sem_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) {
				uint32_t ctrl   = payload[2];
				sem.sem_op      = (ctrl >> 29u) & 0x7u; // SEM_SEL
				sem.signal_type = ((ctrl >> 20u) & 0x1u) != 0;
				sem.mailbox_type= ((ctrl >> 21u) & 0x1u) != 0;
			}
			return sem;
		}

		// ─── DISPATCH_DRAW_PREAMBLE / DISPATCH_DRAW ───────────────────────────
		case IT_DISPATCH_DRAW_PREAMBLE:
		case IT_DISPATCH_DRAW: {
			m_stats.dispatch_packets++;
			DispatchDrawPacket dd{};
			dd.preamble = (header.opcode == IT_DISPATCH_DRAW_PREAMBLE);
			if (payload_dw >= 3) {
				dd.dim_x = payload[0];
				dd.dim_y = payload[1];
				dd.dim_z = payload[2];
			}
			if (payload_dw >= 4) {
				dd.ring_offset = payload[3];
			}
			return dd;
		}

		// ─── COPY_DATA ────────────────────────────────────────────────────────
		case IT_COPY_DATA: {
			m_stats.copy_data_packets++;
			CopyDataPacket cd{};
			if (payload_dw >= 1) {
				uint32_t ctrl = payload[0];
				cd.src_sel    = (ctrl >> 0u)  & 0xFu;
				cd.dst_sel    = (ctrl >> 8u)  & 0xFu;
				cd.count_sel  = (ctrl >> 16u) & 0x1u; // 0=32-bit, 1=64-bit
				cd.wr_confirm = (ctrl >> 21u) & 0x1u;
			}
			if (payload_dw >= 3) {
				cd.src_addr = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
			}
			if (payload_dw >= 5) {
				cd.dst_addr = (static_cast<uint64_t>(payload[4]) << 32u) | payload[3];
			}
			return cd;
		}

		// ─── CP_DMA / DMA_DATA ────────────────────────────────────────────────
		case IT_CP_DMA:
		case IT_DMA_DATA: {
			m_stats.dma_packets++;
			DmaCopyPacket dma{};
			if (header.opcode == IT_DMA_DATA) {
				// GFX9 DMA_DATA encoding: 7-dword packet
				if (payload_dw >= 1) {
					uint32_t ctrl = payload[0];
					dma.src_sel   = (ctrl >> 29u) & 0x3u;
					dma.dst_sel   = (ctrl >> 20u) & 0x3u;
					dma.raw_wait  = ((ctrl >> 26u) & 0x1u) != 0;
				}
				if (payload_dw >= 3) {
					dma.src_addr  = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
				}
				if (payload_dw >= 5) {
					dma.dst_addr  = (static_cast<uint64_t>(payload[4]) << 32u) | payload[3];
				}
				if (payload_dw >= 6) {
					dma.num_bytes = payload[5] & 0x1FFFFFu;
				}
			} else {
				// Legacy CP_DMA (GFX6-8): 5-dword packet
				if (payload_dw >= 4) {
					dma.src_addr  = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
					dma.dst_addr  = (static_cast<uint64_t>(payload[3]) << 32u) | payload[2];
					dma.num_bytes = (payload_dw >= 5) ? (payload[4] & 0x1FFFFFu) : 0;
				}
			}
			return dma;
		}

		// ─── PFP_SYNC_ME: Stall PFP until ME catches up ───────────────────────
		case IT_PFP_SYNC_ME: {
			PfpSyncMePacket pfp{};
			if (payload_dw >= 1) pfp.dummy = payload[0];
			return pfp;
		}

		// ─── SURFACE_SYNC (legacy GFX6-8 flush) ──────────────────────────────
		case IT_SURFACE_SYNC: {
			m_stats.barrier_packets++;
			SurfaceSyncPacket ss{};
			if (payload_dw >= 1) ss.cp_coher_cntl = payload[0];
			if (payload_dw >= 2) ss.cp_coher_size  = payload[1];
			if (payload_dw >= 3) ss.cp_coher_base  = payload[2];
			if (payload_dw >= 4) ss.poll_interval  = payload[3];
			return ss;
		}

		// ─── EVENT_WRITE / EVENT_WRITE_EOP / EVENT_WRITE_EOS ─────────────────
		case IT_EVENT_WRITE:
		case IT_EVENT_WRITE_EOP:
		case IT_EVENT_WRITE_EOS: {
			m_stats.event_packets++;
			EventPacket evt{};
			if (payload_dw >= 1) {
				uint32_t ev0 = payload[0];
				evt.event_type  = ev0 & 0x3Fu;
				evt.event_index = (ev0 >> 8u) & 0xFu;
			}
			evt.write_eop = (header.opcode == IT_EVENT_WRITE_EOP);
			evt.write_eos = (header.opcode == IT_EVENT_WRITE_EOS);
			if (payload_dw >= 3) {
				evt.dst_gpu_addr = (static_cast<uint64_t>(payload[2]) << 32u) | (payload[1] & ~3u);
			}
			if (payload_dw >= 4) {
				uint32_t ev3 = payload[3];
				evt.data_sel = (ev3 >> 29u) & 0x7u;
				evt.int_sel  = (ev3 >> 24u) & 0x3u;
			}
			if (payload_dw >= 5) {
				evt.value = static_cast<uint64_t>(payload[4]);
			}
			if (payload_dw >= 6) {
				evt.value |= (static_cast<uint64_t>(payload[5]) << 32u);
			}
			return evt;
		}

		// ─── RELEASE_MEM: End-of-pipeline fence write ────────────────────────
		case IT_RELEASE_MEM: {
			m_stats.release_mem_packets++;
			ReleaseMemPacket rm{};
			if (payload_dw >= 1) {
				uint32_t w0    = payload[0];
				rm.event_type  = w0 & 0x3Fu;
				rm.event_index = (w0 >> 8u) & 0xFu;
			}
			if (payload_dw >= 2) {
				uint32_t w1 = payload[1];
				rm.data_sel = (w1 >> 29u) & 0x7u;
				rm.int_sel  = (w1 >> 24u) & 0x3u;
				rm.gcr_cntl = w1 & 0x7FFFFFu;
			}
			if (payload_dw >= 4) {
				rm.dst_gpu_addr = (static_cast<uint64_t>(payload[3]) << 32u) | payload[2];
			}
			if (payload_dw >= 5) {
				rm.value = static_cast<uint64_t>(payload[4]);
			}
			if (payload_dw >= 6) {
				rm.value |= (static_cast<uint64_t>(payload[5]) << 32u);
			}
			return rm;
		}

		// ─── ACQUIRE_MEM: Cache flush/invalidation ────────────────────────────
		case IT_ACQUIRE_MEM: {
			m_stats.barrier_packets++;
			BarrierPacket barrier{};
			if (payload_dw >= 1) barrier.cp_coher_cntl = payload[0];
			if (payload_dw >= 2) barrier.cp_coher_size  = payload[1];
			if (payload_dw >= 4) {
				barrier.cp_coher_base = (static_cast<uint64_t>(payload[3]) << 32u) | payload[2];
			}
			if (payload_dw >= 5) barrier.poll_interval = payload[4];
			// Determine logical flush directions from coherency bits
			barrier.flush_inv_cb = ((barrier.cp_coher_cntl & 0x00040000u) != 0); // CB_ACTION_ENA
			barrier.flush_inv_db = ((barrier.cp_coher_cntl & 0x00080000u) != 0); // DB_ACTION_ENA
			return barrier;
		}

		// ─── REWIND: Constant engine ring rewind ─────────────────────────────
		case IT_REWIND: {
			RewindPacket rwd{};
			if (payload_dw >= 1) rwd.rewind_offset = payload[0];
			return rwd;
		}

		// ─── SET_CONTEXT_REG / SET_SH_REG / SET_CONFIG_REG / SET_UCONFIG_REG ─
		case IT_SET_CONTEXT_REG:
		case IT_SET_SH_REG:
		case IT_SET_CONFIG_REG:
		case IT_SET_UCONFIG_REG:
		case IT_SET_UCONFIG_REG_INDEX: {
			SetRegisterPacket reg{};
			if (payload_dw >= 1) {
				reg.reg_offset     = payload[0] & 0xFFFFu;
				reg.count          = payload_dw - 1u;
				reg.values         = (payload_dw > 1) ? payload + 1 : nullptr;
				reg.is_context_reg = (header.opcode == IT_SET_CONTEXT_REG);
				reg.is_sh_reg      = (header.opcode == IT_SET_SH_REG);
				reg.is_config_reg  = (header.opcode == IT_SET_CONFIG_REG);
				reg.is_uconfig_reg = (header.opcode == IT_SET_UCONFIG_REG ||
				                      header.opcode == IT_SET_UCONFIG_REG_INDEX);
			}
			return reg;
		}

		// ─── SET_SH_REG_INDIRECT / SET_UCONFIG_REG_INDIRECT / SET_CONTEXT_REG_INDIRECT
		case IT_SET_SH_REG_INDIRECT:
		case IT_SET_UCONFIG_REG_INDIRECT:
		case IT_SET_CONTEXT_REG_INDIRECT: {
			SetRegisterPacket reg{};
			if (payload_dw >= 1) {
				reg.reg_offset     = payload[0] & 0xFFFFu;
				reg.count          = payload_dw - 1u;
				reg.values         = (payload_dw > 1) ? payload + 1 : nullptr;
				reg.is_sh_reg      = (header.opcode == IT_SET_SH_REG_INDIRECT);
				reg.is_uconfig_reg = (header.opcode == IT_SET_UCONFIG_REG_INDIRECT);
				reg.is_context_reg = (header.opcode == IT_SET_CONTEXT_REG_INDIRECT);
			}
			return reg;
		}

		// ─── SET_QUEUE_REG (GFX10+) ───────────────────────────────────────────
		case IT_SET_QUEUE_REG: {
			SetQueueRegPacket qr{};
			if (payload_dw >= 1) qr.reg_offset = payload[0] & 0xFFFFu;
			if (payload_dw >= 2) qr.value       = payload[1];
			return qr;
		}

		// ─── WRITE_CONST_RAM: Write to CE RAM ────────────────────────────────
		case IT_WRITE_CONST_RAM: {
			m_stats.ce_packets++;
			WriteConstRamPacket wc{};
			if (payload_dw >= 1) {
				wc.dst_offset_dw = (payload[0] >> 2u) & 0xFFFFu; // Byte offset → dword offset
			}
			if (payload_dw > 1) {
				wc.data     = payload + 1;
				wc.count_dw = payload_dw - 1u;
			}
			return wc;
		}

		// ─── DUMP_CONST_RAM: DMA from CE RAM to memory ───────────────────────
		case IT_DUMP_CONST_RAM: {
			m_stats.ce_packets++;
			DumpConstRamPacket dc{};
			if (payload_dw >= 1) {
				dc.src_offset_dw  = (payload[0] >> 2u) & 0xFFFFu;
				dc.cache_policy   = ((payload[0] >> 25u) & 0x1u) != 0;
				dc.count_dw       = (payload[0] >> 16u) & 0x1FFu;
			}
			if (payload_dw >= 3) {
				dc.dst_gpu_addr   = (static_cast<uint64_t>(payload[2]) << 32u) | payload[1];
			}
			return dc;
		}

		// ─── INCREMENT_CE_COUNTER / INCREMENT_DE_COUNTER ─────────────────────
		case IT_INCREMENT_CE_COUNTER:
		case IT_INCREMENT_DE_COUNTER: {
			m_stats.ce_packets++;
			CeCounterPacket cc{};
			if (payload_dw >= 1) cc.dummy = payload[0];
			return cc;
		}

		// ─── WAIT_ON_CE_COUNTER / WAIT_ON_DE_COUNTER_DIFF ────────────────────
		case IT_WAIT_ON_CE_COUNTER:
		case IT_WAIT_ON_DE_COUNTER_DIFF: {
			m_stats.ce_packets++;
			WaitCeCounterPacket wc{};
			if (payload_dw >= 1) wc.wait_count = payload[0];
			return wc;
		}

		// ─── GET_LOD_STATS: LOD feedback readback ─────────────────────────────
		case IT_GET_LOD_STATS: {
			GetLodStatsPacket ls{};
			if (payload_dw >= 2) {
				ls.dst_gpu_addr = (static_cast<uint64_t>(payload[1]) << 32u) | payload[0];
			}
			if (payload_dw >= 3) ls.chunk_id    = payload[2];
			if (payload_dw >= 4) ls.buf_size_dw = payload[3];
			return ls;
		}

		// ─── Anything not explicitly handled ──────────────────────────────────
		default:
			m_stats.unknown_packets++;
			return SetRegisterPacket{};
	}
}

} // namespace Libs::Graphics::Pm4
