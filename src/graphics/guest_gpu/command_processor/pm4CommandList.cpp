// pm4CommandList.cpp
//
// Backend-independent GPU command list data structure for Phase K.

#include "graphics/guest_gpu/command_processor/pm4CommandList.h"

#include <algorithm>
#include <cstring>

namespace Libs::Graphics::Pm4 {

void Pm4CommandList::RecordDrawNonIndexed(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
	CmdDrawNonIndexed cmd{};
	cmd.vertex_count   = vertex_count;
	cmd.instance_count = instance_count;
	cmd.first_vertex   = first_vertex;
	cmd.first_instance = first_instance;

	m_commands.push_back(GenericCommand{CommandType::DrawNonIndexed, cmd});
}

void Pm4CommandList::RecordDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance, uint64_t index_addr, uint32_t index_type) {
	CmdDrawIndexed cmd{};
	cmd.index_count    = index_count;
	cmd.instance_count = instance_count;
	cmd.first_index    = first_index;
	cmd.vertex_offset  = vertex_offset;
	cmd.first_instance = first_instance;
	cmd.index_gpu_addr = index_addr;
	cmd.index_type     = index_type;

	m_commands.push_back(GenericCommand{CommandType::DrawIndexed, cmd});
}

void Pm4CommandList::RecordDrawIndirect(uint64_t args_addr, uint32_t draw_count, uint32_t stride_bytes, bool indexed) {
	CmdDrawIndirect cmd{};
	cmd.args_gpu_addr = args_addr;
	cmd.draw_count    = draw_count;
	cmd.stride_bytes  = stride_bytes;
	cmd.indexed       = indexed;

	m_commands.push_back(GenericCommand{CommandType::DrawIndirect, cmd});
}

void Pm4CommandList::RecordDispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) {
	CmdDispatchDirect cmd{};
	cmd.group_count_x = group_x;
	cmd.group_count_y = group_y;
	cmd.group_count_z = group_z;

	m_commands.push_back(GenericCommand{CommandType::DispatchDirect, cmd});
}

void Pm4CommandList::RecordDispatchIndirect(uint64_t args_addr) {
	CmdDispatchIndirect cmd{};
	cmd.args_gpu_addr = args_addr;

	m_commands.push_back(GenericCommand{CommandType::DispatchIndirect, cmd});
}

void Pm4CommandList::RecordDmaCopy(uint64_t src_addr, uint64_t dst_addr, uint32_t num_bytes) {
	CmdDmaCopy cmd{};
	cmd.src_gpu_addr = src_addr;
	cmd.dst_gpu_addr = dst_addr;
	cmd.num_bytes    = num_bytes;

	m_commands.push_back(GenericCommand{CommandType::DmaCopy, cmd});
}

void Pm4CommandList::RecordClear(const float color[4], float depth, uint8_t stencil, bool clear_color, bool clear_depth, bool clear_stencil) {
	CmdClearRenderTarget cmd{};
	if (color) {
		std::memcpy(cmd.color, color, sizeof(float) * 4);
	}
	cmd.depth         = depth;
	cmd.stencil       = stencil;
	cmd.clear_color   = clear_color;
	cmd.clear_depth   = clear_depth;
	cmd.clear_stencil = clear_stencil;

	m_commands.push_back(GenericCommand{CommandType::ClearRenderTarget, cmd});
}

void Pm4CommandList::RecordBarrier(bool flush_cb, bool flush_db, bool inv_l2) {
	CmdPipelineBarrier cmd{};
	cmd.flush_cb = flush_cb;
	cmd.flush_db = flush_db;
	cmd.inv_l2   = inv_l2;

	m_commands.push_back(GenericCommand{CommandType::PipelineBarrier, cmd});
}

void Pm4CommandList::RecordSetEvent(uint32_t event_type, uint64_t signal_addr, uint64_t value) {
	CmdSetEvent cmd{};
	cmd.event_type     = event_type;
	cmd.signal_address = signal_addr;
	cmd.value          = value;

	m_commands.push_back(GenericCommand{CommandType::SetEvent, cmd});
}

void Pm4CommandList::RecordTimestampQuery(uint64_t query_addr) {
	CmdTimestampQuery cmd{};
	cmd.query_address = query_addr;

	m_commands.push_back(GenericCommand{CommandType::TimestampQuery, cmd});
}

void Pm4CommandList::RecordSetRegister(uint32_t reg_offset, const uint32_t* values, uint32_t count, bool is_sh) {
	CmdSetRegisterState cmd{};
	cmd.reg_offset = reg_offset;
	cmd.count      = (count > 16) ? 16 : count;
	cmd.is_sh_reg  = is_sh;
	if (values && count > 0) {
		std::memcpy(cmd.values, values, cmd.count * sizeof(uint32_t));
	}

	m_commands.push_back(GenericCommand{CommandType::SetRegisterState, cmd});
}

void Pm4CommandList::RecordSetPredication(uint64_t query_addr, bool enable, bool hint_draw) {
	CmdSetPredication cmd{};
	cmd.query_gpu_addr = query_addr;
	cmd.pred_enable    = enable;
	cmd.hint_draw      = hint_draw;

	m_commands.push_back(GenericCommand{CommandType::SetPredication, cmd});
}

void Pm4CommandList::RecordMemSemaphore(uint64_t sem_addr, uint32_t op) {
	CmdMemSemaphore cmd{};
	cmd.sem_gpu_addr = sem_addr;
	cmd.sem_op       = op;

	m_commands.push_back(GenericCommand{CommandType::MemSemaphore, cmd});
}

void Pm4CommandList::RecordSetIndexType(uint32_t index_type) {
	CmdSetIndexType cmd{};
	cmd.index_type = index_type;

	m_commands.push_back(GenericCommand{CommandType::SetIndexType, cmd});
}

void Pm4CommandList::Append(const Pm4CommandList& other) {
	m_commands.insert(m_commands.end(), other.m_commands.begin(), other.m_commands.end());
}

} // namespace Libs::Graphics::Pm4
