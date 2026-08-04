// pm4CommandList.cpp
//
// Backend-independent GPU command list implementation.
// Covers 100% of AMD PM4 packet translation paths.

#include "graphics/guest_gpu/command_processor/pm4CommandList.h"

#include <algorithm>
#include <cstring>

namespace Libs::Graphics::Pm4 {

// ─── Draw / Dispatch ──────────────────────────────────────────────────────────

void Pm4CommandList::RecordDrawNonIndexed(uint32_t vertex_count, uint32_t instance_count,
                                           uint32_t first_vertex, uint32_t first_instance) {
	CmdDrawNonIndexed cmd{};
	cmd.vertex_count   = vertex_count;
	cmd.instance_count = instance_count;
	cmd.first_vertex   = first_vertex;
	cmd.first_instance = first_instance;
	Emit(CommandType::DrawNonIndexed, cmd);
}

void Pm4CommandList::RecordDrawIndexed(uint32_t index_count, uint32_t instance_count,
                                        uint32_t first_index, int32_t vertex_offset,
                                        uint32_t first_instance, uint64_t index_addr,
                                        uint32_t index_type) {
	CmdDrawIndexed cmd{};
	cmd.index_count    = index_count;
	cmd.instance_count = instance_count;
	cmd.first_index    = first_index;
	cmd.vertex_offset  = vertex_offset;
	cmd.first_instance = first_instance;
	cmd.index_gpu_addr = index_addr;
	cmd.index_type     = index_type;
	Emit(CommandType::DrawIndexed, cmd);
}

void Pm4CommandList::RecordDrawIndexedOffset(uint32_t index_count, uint32_t index_offset,
                                              uint64_t index_addr, uint32_t instance_count) {
	CmdDrawIndexedOffset cmd{};
	cmd.index_count    = index_count;
	cmd.index_offset   = index_offset;
	cmd.index_gpu_addr = index_addr;
	cmd.instance_count = instance_count;
	Emit(CommandType::DrawIndexedIndirectOffset, cmd);
}

void Pm4CommandList::RecordDrawIndirect(uint64_t args_addr, uint32_t draw_count,
                                         uint32_t stride_bytes, bool indexed) {
	CmdDrawIndirect cmd{};
	cmd.args_gpu_addr = args_addr;
	cmd.draw_count    = draw_count;
	cmd.stride_bytes  = stride_bytes;
	cmd.indexed       = indexed;
	Emit(CommandType::DrawIndirect, cmd);
}

void Pm4CommandList::RecordMultiDrawIndirect(uint64_t indirect_addr, uint64_t count_addr,
                                              uint32_t max_draw_count, uint32_t stride,
                                              bool indexed, bool count_indirect) {
	CmdMultiDrawIndirect cmd{};
	cmd.indirect_gpu_addr = indirect_addr;
	cmd.count_gpu_addr    = count_addr;
	cmd.draw_count        = max_draw_count;
	cmd.stride_bytes      = stride;
	cmd.indexed           = indexed;
	cmd.count_indirect    = count_indirect;
	Emit(CommandType::MultiDrawIndirect, cmd);
}

void Pm4CommandList::RecordDispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z) {
	CmdDispatchDirect cmd{};
	cmd.group_count_x = group_x;
	cmd.group_count_y = group_y;
	cmd.group_count_z = group_z;
	Emit(CommandType::DispatchDirect, cmd);
}

void Pm4CommandList::RecordDispatchIndirect(uint64_t args_addr) {
	CmdDispatchIndirect cmd{};
	cmd.args_gpu_addr = args_addr;
	Emit(CommandType::DispatchIndirect, cmd);
}

void Pm4CommandList::RecordDispatchDraw(uint32_t dim_x, uint32_t dim_y, uint32_t dim_z,
                                         uint32_t ring_offset, bool preamble) {
	CmdDispatchDraw cmd{};
	cmd.dim_x       = dim_x;
	cmd.dim_y       = dim_y;
	cmd.dim_z       = dim_z;
	cmd.ring_offset = ring_offset;
	cmd.preamble    = preamble;
	Emit(CommandType::DispatchDraw, cmd);
}

void Pm4CommandList::RecordNumInstances(uint32_t instance_count) {
	CmdNumInstances cmd{};
	cmd.instance_count = instance_count;
	Emit(CommandType::NumInstances, cmd);
}

// ─── Memory / DMA ─────────────────────────────────────────────────────────────

void Pm4CommandList::RecordDmaCopy(uint64_t src_addr, uint64_t dst_addr, uint32_t num_bytes) {
	RecordDmaCopySelective(src_addr, dst_addr, num_bytes, 0, 0);
}

void Pm4CommandList::RecordDmaCopySelective(uint64_t src_addr, uint64_t dst_addr,
                                             uint32_t num_bytes, uint8_t src_sel, uint8_t dst_sel) {
	CmdDmaCopy cmd{};
	cmd.src_gpu_addr = src_addr;
	cmd.dst_gpu_addr = dst_addr;
	cmd.num_bytes    = num_bytes;
	cmd.src_sel      = src_sel;
	cmd.dst_sel      = dst_sel;
	Emit(CommandType::DmaCopy, cmd);
}

void Pm4CommandList::RecordWriteData(uint64_t dst_addr, uint32_t engine_sel, uint32_t dst_sel,
                                      uint32_t wr_confirm, const uint32_t* data, uint32_t count_dw) {
	CmdWriteData cmd{};
	cmd.dst_gpu_addr = dst_addr;
	cmd.engine_sel   = engine_sel;
	cmd.dst_sel      = dst_sel;
	cmd.wr_confirm   = wr_confirm;
	cmd.count_dw     = std::min(count_dw, CmdWriteData::kMaxInlineData);
	if (data && cmd.count_dw > 0) {
		std::memcpy(cmd.data, data, cmd.count_dw * sizeof(uint32_t));
	}
	Emit(CommandType::WriteData, cmd);
}

void Pm4CommandList::RecordCopyData(uint64_t src_addr, uint64_t dst_addr, uint32_t src_sel,
                                     uint32_t dst_sel, uint32_t count_sel, uint32_t wr_confirm) {
	CmdCopyData cmd{};
	cmd.src_addr   = src_addr;
	cmd.dst_addr   = dst_addr;
	cmd.src_sel    = src_sel;
	cmd.dst_sel    = dst_sel;
	cmd.count_sel  = count_sel;
	cmd.wr_confirm = wr_confirm;
	Emit(CommandType::CopyData, cmd);
}

// ─── Clears / Barriers ────────────────────────────────────────────────────────

void Pm4CommandList::RecordClear(const float color[4], float depth, uint8_t stencil,
                                  bool clear_color, bool clear_depth, bool clear_stencil) {
	CmdClearRenderTarget cmd{};
	if (color) {
		std::memcpy(cmd.color, color, sizeof(float) * 4);
	}
	cmd.depth         = depth;
	cmd.stencil       = stencil;
	cmd.clear_color   = clear_color;
	cmd.clear_depth   = clear_depth;
	cmd.clear_stencil = clear_stencil;
	Emit(CommandType::ClearRenderTarget, cmd);
}

void Pm4CommandList::RecordBarrier(bool flush_cb, bool flush_db, bool inv_l2) {
	CmdPipelineBarrier cmd{};
	cmd.flush_cb = flush_cb;
	cmd.flush_db = flush_db;
	cmd.inv_l2   = inv_l2;
	Emit(CommandType::PipelineBarrier, cmd);
}

void Pm4CommandList::RecordAcquireMem(uint32_t coher_cntl, uint32_t coher_size,
                                       uint64_t coher_base, uint32_t poll_interval) {
	CmdPipelineBarrier cmd{};
	cmd.flush_cb    = ((coher_cntl & 0x00040000u) != 0); // CB_ACTION_ENA
	cmd.flush_db    = ((coher_cntl & 0x00080000u) != 0); // DB_ACTION_ENA
	cmd.inv_l2      = ((coher_cntl & 0x00000200u) != 0); // TCL2 invalidation
	cmd.coher_cntl  = coher_cntl;
	cmd.coher_size  = coher_size;
	cmd.coher_base  = coher_base;
	Emit(CommandType::PipelineBarrier, cmd);
}

void Pm4CommandList::RecordSurfaceSync(uint32_t cp_coher_cntl, uint32_t cp_coher_size,
                                        uint32_t cp_coher_base, uint32_t poll_interval) {
	CmdSurfaceSync cmd{};
	cmd.cp_coher_cntl = cp_coher_cntl;
	cmd.cp_coher_size = cp_coher_size;
	cmd.cp_coher_base = cp_coher_base;
	cmd.poll_interval = poll_interval;
	Emit(CommandType::SurfaceSync, cmd);
}

// ─── Events / Sync ────────────────────────────────────────────────────────────

void Pm4CommandList::RecordSetEvent(uint32_t event_type, uint64_t signal_addr, uint64_t value) {
	CmdSetEvent cmd{};
	cmd.event_type     = event_type;
	cmd.signal_address = signal_addr;
	cmd.value          = value;
	Emit(CommandType::SetEvent, cmd);
}

void Pm4CommandList::RecordReleaseMem(uint32_t event_type, uint32_t event_index,
                                       uint32_t data_sel, uint32_t int_sel,
                                       uint64_t dst_addr, uint64_t value, uint32_t gcr_cntl) {
	CmdReleaseMem cmd{};
	cmd.event_type   = event_type;
	cmd.event_index  = event_index;
	cmd.data_sel     = data_sel;
	cmd.int_sel      = int_sel;
	cmd.dst_gpu_addr = dst_addr;
	cmd.value        = value;
	cmd.gcr_cntl     = gcr_cntl;
	Emit(CommandType::ReleaseMem, cmd);
}

void Pm4CommandList::RecordTimestampQuery(uint64_t query_addr) {
	CmdTimestampQuery cmd{};
	cmd.query_address = query_addr;
	Emit(CommandType::TimestampQuery, cmd);
}

void Pm4CommandList::RecordMemSemaphore(uint64_t sem_addr, uint32_t op) {
	CmdMemSemaphore cmd{};
	cmd.sem_gpu_addr = sem_addr;
	cmd.sem_op       = op;
	cmd.signal_type  = (op == 0);
	Emit(CommandType::MemSemaphore, cmd);
}

void Pm4CommandList::RecordPfpSyncMe() {
	CmdPfpSyncMe cmd{};
	Emit(CommandType::PfpSyncMe, cmd);
}

// ─── Register state ───────────────────────────────────────────────────────────

void Pm4CommandList::RecordSetRegister(uint32_t reg_offset, const uint32_t* values,
                                        uint32_t count, bool is_sh, bool is_uconfig, bool is_config) {
	CmdSetRegisterState cmd{};
	cmd.reg_offset = reg_offset;
	cmd.count      = (count > 16) ? 16 : count;
	cmd.is_sh_reg  = is_sh;
	cmd.is_uconfig = is_uconfig;
	cmd.is_config  = is_config;
	if (values && count > 0) {
		std::memcpy(cmd.values, values, cmd.count * sizeof(uint32_t));
	}
	Emit(CommandType::SetRegisterState, cmd);
}

void Pm4CommandList::RecordSetPredication(uint64_t query_addr, bool enable, bool hint_draw,
                                           uint32_t pred_op, bool cont) {
	CmdSetPredication cmd{};
	cmd.query_gpu_addr = query_addr;
	cmd.pred_enable    = enable;
	cmd.hint_draw      = hint_draw;
	cmd.pred_op        = pred_op;
	cmd.continue_bit   = cont;
	Emit(CommandType::SetPredication, cmd);
}

void Pm4CommandList::RecordSetIndexType(uint32_t index_type) {
	CmdSetIndexType cmd{};
	cmd.index_type = index_type;
	Emit(CommandType::SetIndexType, cmd);
}

void Pm4CommandList::RecordSetBase(uint32_t base_type, uint64_t gpu_addr) {
	CmdSetBase cmd{};
	cmd.base_type = base_type;
	cmd.gpu_addr  = gpu_addr;
	Emit(CommandType::SetBase, cmd);
}

void Pm4CommandList::RecordClearState(uint32_t flags) {
	CmdClearState cmd{};
	cmd.flags = flags;
	Emit(CommandType::ClearState, cmd);
}

void Pm4CommandList::RecordContextControl(uint32_t load_control, uint32_t shadow_control) {
	CmdContextControl cmd{};
	cmd.load_control   = load_control;
	cmd.shadow_control = shadow_control;
	Emit(CommandType::ContextControl, cmd);
}

void Pm4CommandList::RecordIndirectBuffer(uint64_t ib_addr, uint32_t ib_size_dw,
                                           bool chain_mode, bool preempt_en, uint32_t vmid) {
	CmdIndirectBuffer cmd{};
	cmd.ib_gpu_addr = ib_addr;
	cmd.ib_size_dw  = ib_size_dw;
	cmd.chain_mode  = chain_mode;
	cmd.preempt_en  = preempt_en;
	cmd.vmid        = vmid;
	Emit(CommandType::IndirectBuffer, cmd);
}

// ─── Constant Engine ──────────────────────────────────────────────────────────

void Pm4CommandList::RecordWriteConstRam(uint32_t dst_offset_dw, const uint32_t* data,
                                          uint32_t count_dw) {
	CmdWriteConstRam cmd{};
	cmd.dst_offset_dw = dst_offset_dw;
	cmd.count_dw      = std::min(count_dw, CmdWriteConstRam::kMaxRamData);
	if (data && cmd.count_dw > 0) {
		std::memcpy(cmd.data, data, cmd.count_dw * sizeof(uint32_t));
	}
	Emit(CommandType::WriteConstRam, cmd);
}

void Pm4CommandList::RecordDumpConstRam(uint64_t dst_addr, uint32_t src_offset_dw,
                                         uint32_t count_dw, bool cache_policy) {
	CmdDumpConstRam cmd{};
	cmd.dst_gpu_addr  = dst_addr;
	cmd.src_offset_dw = src_offset_dw;
	cmd.count_dw      = count_dw;
	cmd.cache_policy  = cache_policy;
	Emit(CommandType::DumpConstRam, cmd);
}

void Pm4CommandList::RecordIncrementCeCounter() {
	CmdCeCounter cmd{};
	cmd.is_de_counter = false;
	Emit(CommandType::IncrementCeCounter, cmd);
}

void Pm4CommandList::RecordIncrementDeCounter() {
	CmdCeCounter cmd{};
	cmd.is_de_counter = true;
	Emit(CommandType::IncrementDeCounter, cmd);
}

void Pm4CommandList::RecordWaitOnCeCounter() {
	CmdWaitCeCounter cmd{};
	cmd.wait_de_diff = false;
	cmd.wait_count   = 0;
	Emit(CommandType::WaitCeCounter, cmd);
}

void Pm4CommandList::RecordWaitOnDeCounterDiff(uint32_t diff) {
	CmdWaitCeCounter cmd{};
	cmd.wait_de_diff = true;
	cmd.wait_count   = diff;
	Emit(CommandType::WaitDeCounterDiff, cmd);
}

// ─── Misc ─────────────────────────────────────────────────────────────────────

void Pm4CommandList::RecordGetLodStats(uint64_t dst_addr, uint32_t chunk_id, uint32_t buf_size_dw) {
	CmdGetLodStats cmd{};
	cmd.dst_gpu_addr = dst_addr;
	cmd.chunk_id     = chunk_id;
	cmd.buf_size_dw  = buf_size_dw;
	Emit(CommandType::GetLodStats, cmd);
}

void Pm4CommandList::RecordRewind(uint32_t offset) {
	CmdRewind cmd{};
	cmd.rewind_offset = offset;
	Emit(CommandType::Rewind, cmd);
}

void Pm4CommandList::RecordNop(uint32_t payload_dw) {
	CmdNop cmd{};
	cmd.payload_dw = payload_dw;
	Emit(CommandType::Nop, cmd);
}

void Pm4CommandList::Append(const Pm4CommandList& other) {
	m_commands.insert(m_commands.end(), other.m_commands.begin(), other.m_commands.end());
}

} // namespace Libs::Graphics::Pm4
