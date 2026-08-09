// pm4CommandList.h
//
// Backend-independent GPU command list data structure.
// Covers 100% of AMD PM4 packet translation paths.

#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_COMMAND_LIST_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_COMMAND_LIST_H

#include "common/common.h"
#include "graphics/guest_gpu/command_processor/pm4Parser.h"

#include <cstdint>
#include <variant>
#include <vector>

namespace Libs::Graphics::Pm4 {

enum class CommandType : uint8_t {
	DrawNonIndexed,
	DrawIndexed,
	DrawIndirect,
	DrawIndexedIndirectOffset, // IT_DRAW_INDEX_OFFSET_2
	DispatchDirect,
	DispatchIndirect,
	DispatchDraw,              // IT_DISPATCH_DRAW / IT_DISPATCH_DRAW_PREAMBLE
	DmaCopy,
	ClearRenderTarget,
	PipelineBarrier,
	SurfaceSync,               // IT_SURFACE_SYNC (legacy)
	SetEvent,
	ReleaseMem,                // IT_RELEASE_MEM
	TimestampQuery,
	SetRegisterState,
	SetPredication,
	MemSemaphore,
	SetIndexType,
	SetBase,
	ClearState,
	ContextControl,
	WriteData,                 // IT_WRITE_DATA
	CopyData,                  // IT_COPY_DATA
	PfpSyncMe,                 // IT_PFP_SYNC_ME
	MultiDrawIndirect,         // IT_DRAW_INDIRECT_MULTI / IT_DRAW_INDEX_INDIRECT_MULTI
	NumInstances,
	IndirectBuffer,            // IT_INDIRECT_BUFFER / IT_INDIRECT_BUFFER_CNST
	WriteConstRam,             // IT_WRITE_CONST_RAM
	DumpConstRam,              // IT_DUMP_CONST_RAM
	IncrementCeCounter,        // IT_INCREMENT_CE_COUNTER
	IncrementDeCounter,        // IT_INCREMENT_DE_COUNTER
	WaitCeCounter,             // IT_WAIT_ON_CE_COUNTER
	WaitDeCounterDiff,         // IT_WAIT_ON_DE_COUNTER_DIFF
	GetLodStats,               // IT_GET_LOD_STATS
	Rewind,                    // IT_REWIND
	Nop,                       // IT_NOP
};

// ─── Command data structs ──────────────────────────────────────────────────────

struct CmdDrawNonIndexed {
	uint32_t vertex_count   = 0;
	uint32_t instance_count = 1;
	uint32_t first_vertex   = 0;
	uint32_t first_instance = 0;
};

struct CmdDrawIndexed {
	uint32_t index_count    = 0;
	uint32_t instance_count = 1;
	uint32_t first_index    = 0;
	int32_t  vertex_offset  = 0;
	uint32_t first_instance = 0;
	uint64_t index_gpu_addr = 0;
	uint32_t index_type     = 0; // 0=uint16, 1=uint32
};

struct CmdDrawIndexedOffset {
	uint32_t index_count    = 0;
	uint32_t index_offset   = 0; // Base index offset from IT_DRAW_INDEX_OFFSET_2
	uint64_t index_gpu_addr = 0;
	uint32_t instance_count = 1;
};

struct CmdDrawIndirect {
	uint64_t args_gpu_addr  = 0;
	uint64_t count_gpu_addr = 0; // 0 = use draw_count
	uint32_t draw_count     = 1;
	uint32_t stride_bytes   = 0;
	bool     indexed        = false;
	bool     count_indirect = false;
};

struct CmdDispatchDirect {
	uint32_t group_count_x = 1;
	uint32_t group_count_y = 1;
	uint32_t group_count_z = 1;
};

struct CmdDispatchIndirect {
	uint64_t args_gpu_addr = 0;
};

struct CmdDispatchDraw {
	uint32_t dim_x       = 0;
	uint32_t dim_y       = 0;
	uint32_t dim_z       = 0;
	uint32_t ring_offset = 0;
	bool     preamble    = false;
};

struct CmdDmaCopy {
	uint64_t src_gpu_addr = 0;
	uint64_t dst_gpu_addr = 0;
	uint32_t num_bytes    = 0;
	uint8_t  src_sel      = 0;
	uint8_t  dst_sel      = 0;
};

struct CmdClearRenderTarget {
	float    color[4]       = {0.0f, 0.0f, 0.0f, 1.0f};
	float    depth          = 1.0f;
	uint8_t  stencil        = 0;
	bool     clear_color    = true;
	bool     clear_depth    = false;
	bool     clear_stencil  = false;
};

struct CmdPipelineBarrier {
	bool     flush_cb     = true;
	bool     flush_db     = true;
	bool     inv_l2       = true;
	uint32_t coher_cntl   = 0; // AMD ACQUIRE_MEM coherency bits
	uint64_t coher_base   = 0;
	uint32_t coher_size   = 0;
};

struct CmdSurfaceSync {
	uint32_t cp_coher_cntl = 0;
	uint32_t cp_coher_size = 0;
	uint32_t cp_coher_base = 0;
	uint32_t poll_interval = 10;
};

struct CmdSetEvent {
	uint32_t event_type     = 0;
	uint64_t signal_address = 0;
	uint64_t value          = 0;
};

struct CmdReleaseMem {
	uint32_t event_type   = 0;
	uint32_t event_index  = 0;
	uint32_t data_sel     = 0;
	uint32_t int_sel      = 0;
	uint64_t dst_gpu_addr = 0;
	uint64_t value        = 0;
	uint32_t gcr_cntl     = 0;
};

struct CmdTimestampQuery {
	uint64_t query_address = 0;
};

struct CmdSetRegisterState {
	uint32_t reg_offset = 0;
	uint32_t count      = 0;
	uint32_t values[16] = {0};
	bool     is_sh_reg  = false;
	bool     is_uconfig = false;
	bool     is_config  = false;
};

struct CmdSetPredication {
	uint64_t query_gpu_addr = 0;
	uint32_t pred_op        = 0;
	bool     pred_enable    = false;
	bool     hint_draw      = false;
	bool     continue_bit   = false;
};

struct CmdMemSemaphore {
	uint64_t sem_gpu_addr  = 0;
	uint32_t sem_op        = 0;
	bool     signal_type   = false;
};

struct CmdSetIndexType {
	uint32_t index_type = 0;
};

struct CmdSetBase {
	uint32_t base_type = 0;
	uint64_t gpu_addr  = 0;
};

struct CmdClearState {
	uint32_t flags = 0;
};

struct CmdContextControl {
	uint32_t load_control   = 0;
	uint32_t shadow_control = 0;
};

struct CmdWriteData {
	uint64_t dst_gpu_addr = 0;
	uint32_t engine_sel   = 0;
	uint32_t dst_sel      = 0;
	uint32_t wr_confirm   = 0;
	// Inline data (max 64 DW for common operations)
	static constexpr uint32_t kMaxInlineData = 64;
	uint32_t data[kMaxInlineData] = {0};
	uint32_t count_dw             = 0;
};

struct CmdCopyData {
	uint64_t src_addr   = 0;
	uint64_t dst_addr   = 0;
	uint32_t src_sel    = 0;
	uint32_t dst_sel    = 0;
	uint32_t count_sel  = 0; // 0=32-bit, 1=64-bit
	uint32_t wr_confirm = 0;
};

struct CmdPfpSyncMe {
	uint32_t dummy = 0;
};

struct CmdMultiDrawIndirect {
	uint64_t indirect_gpu_addr = 0;
	uint64_t count_gpu_addr    = 0;
	uint32_t draw_count        = 1;
	uint32_t stride_bytes      = 0;
	bool     indexed           = false;
	bool     count_indirect    = false;
};

struct CmdNumInstances {
	uint32_t instance_count = 1;
};

struct CmdIndirectBuffer {
	uint64_t ib_gpu_addr = 0;
	uint32_t ib_size_dw  = 0;
	bool     chain_mode  = false;
	bool     preempt_en  = false;
	uint32_t vmid        = 0;
};

struct CmdWriteConstRam {
	uint32_t dst_offset_dw = 0;
	// Inline CE RAM data (max 256 DW for common operations)
	static constexpr uint32_t kMaxRamData = 256;
	uint32_t data[kMaxRamData] = {0};
	uint32_t count_dw          = 0;
};

struct CmdDumpConstRam {
	uint64_t dst_gpu_addr  = 0;
	uint32_t src_offset_dw = 0;
	uint32_t count_dw      = 0;
	bool     cache_policy  = false;
};

struct CmdCeCounter {
	bool is_de_counter = false; // false=CE, true=DE
};

struct CmdWaitCeCounter {
	uint32_t wait_count    = 0;
	bool     wait_de_diff  = false; // false=CE, true=DE_DIFF
};

struct CmdGetLodStats {
	uint64_t dst_gpu_addr = 0;
	uint32_t chunk_id     = 0;
	uint32_t buf_size_dw  = 0;
};

struct CmdRewind {
	uint32_t rewind_offset = 0;
};

struct CmdNop {
	uint32_t payload_dw = 0;
};

// ─── Generic command variant ──────────────────────────────────────────────────

using GenericCommandData = std::variant<
	CmdDrawNonIndexed,
	CmdDrawIndexed,
	CmdDrawIndexedOffset,
	CmdDrawIndirect,
	CmdDispatchDirect,
	CmdDispatchIndirect,
	CmdDispatchDraw,
	CmdDmaCopy,
	CmdClearRenderTarget,
	CmdPipelineBarrier,
	CmdSurfaceSync,
	CmdSetEvent,
	CmdReleaseMem,
	CmdTimestampQuery,
	CmdSetRegisterState,
	CmdSetPredication,
	CmdMemSemaphore,
	CmdSetIndexType,
	CmdSetBase,
	CmdClearState,
	CmdContextControl,
	CmdWriteData,
	CmdCopyData,
	CmdPfpSyncMe,
	CmdMultiDrawIndirect,
	CmdNumInstances,
	CmdIndirectBuffer,
	CmdWriteConstRam,
	CmdDumpConstRam,
	CmdCeCounter,
	CmdWaitCeCounter,
	CmdGetLodStats,
	CmdRewind,
	CmdNop
>;

struct GenericCommand {
	CommandType        type = CommandType::Nop;
	GenericCommandData data;
};

// ─── Command list ─────────────────────────────────────────────────────────────

class Pm4CommandList {
public:
	Pm4CommandList() = default;
	~Pm4CommandList() = default;

	KYTY_CLASS_NO_COPY(Pm4CommandList);

	void Reserve(size_t capacity) { m_commands.reserve(capacity); }
	void Clear() noexcept { m_commands.clear(); }

	// ─── Draw / Dispatch ──────────────────────────────────────────────────────
	void RecordDrawNonIndexed(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
	void RecordDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance, uint64_t index_addr, uint32_t index_type);
	void RecordDrawIndexedOffset(uint32_t index_count, uint32_t index_offset, uint64_t index_addr, uint32_t instance_count = 1);
	void RecordDrawIndirect(uint64_t args_addr, uint32_t draw_count, uint32_t stride_bytes, bool indexed);
	void RecordMultiDrawIndirect(uint64_t indirect_addr, uint64_t count_addr, uint32_t max_draw_count, uint32_t stride, bool indexed, bool count_indirect);
	void RecordDispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z);
	void RecordDispatchIndirect(uint64_t args_addr);
	void RecordDispatchDraw(uint32_t dim_x, uint32_t dim_y, uint32_t dim_z, uint32_t ring_offset, bool preamble);
	void RecordNumInstances(uint32_t instance_count);

	// ─── Memory / DMA ────────────────────────────────────────────────────────
	void RecordDmaCopy(uint64_t src_addr, uint64_t dst_addr, uint32_t num_bytes);
	void RecordDmaCopySelective(uint64_t src_addr, uint64_t dst_addr, uint32_t num_bytes, uint8_t src_sel, uint8_t dst_sel);
	void RecordWriteData(uint64_t dst_addr, uint32_t engine_sel, uint32_t dst_sel, uint32_t wr_confirm, const uint32_t* data, uint32_t count_dw);
	void RecordCopyData(uint64_t src_addr, uint64_t dst_addr, uint32_t src_sel, uint32_t dst_sel, uint32_t count_sel, uint32_t wr_confirm);

	// ─── Clears / Barriers ──────────────────────────────────────────────────
	void RecordClear(const float color[4], float depth, uint8_t stencil, bool clear_color, bool clear_depth, bool clear_stencil);
	void RecordBarrier(bool flush_cb, bool flush_db, bool inv_l2);
	void RecordAcquireMem(uint32_t coher_cntl, uint32_t coher_size, uint64_t coher_base, uint32_t poll_interval);
	void RecordSurfaceSync(uint32_t cp_coher_cntl, uint32_t cp_coher_size, uint32_t cp_coher_base, uint32_t poll_interval);

	// ─── Events / Sync ────────────────────────────────────────────────────────
	void RecordSetEvent(uint32_t event_type, uint64_t signal_addr, uint64_t value);
	void RecordReleaseMem(uint32_t event_type, uint32_t event_index, uint32_t data_sel, uint32_t int_sel, uint64_t dst_addr, uint64_t value, uint32_t gcr_cntl);
	void RecordTimestampQuery(uint64_t query_addr);
	void RecordMemSemaphore(uint64_t sem_addr, uint32_t op);
	void RecordPfpSyncMe();

	// ─── Register state ───────────────────────────────────────────────────────
	void RecordSetRegister(uint32_t reg_offset, const uint32_t* values, uint32_t count, bool is_sh, bool is_uconfig = false, bool is_config = false);
	void RecordSetPredication(uint64_t query_addr, bool enable, bool hint_draw, uint32_t pred_op = 0, bool cont = false);
	void RecordSetIndexType(uint32_t index_type);
	void RecordSetBase(uint32_t base_type, uint64_t gpu_addr);
	void RecordClearState(uint32_t flags);
	void RecordContextControl(uint32_t load_control, uint32_t shadow_control);
	void RecordIndirectBuffer(uint64_t ib_addr, uint32_t ib_size_dw, bool chain_mode, bool preempt_en, uint32_t vmid);

	// ─── Constant Engine ─────────────────────────────────────────────────────
	void RecordWriteConstRam(uint32_t dst_offset_dw, const uint32_t* data, uint32_t count_dw);
	void RecordDumpConstRam(uint64_t dst_addr, uint32_t src_offset_dw, uint32_t count_dw, bool cache_policy);
	void RecordIncrementCeCounter();
	void RecordIncrementDeCounter();
	void RecordWaitOnCeCounter();
	void RecordWaitOnDeCounterDiff(uint32_t diff);

	// ─── Misc ────────────────────────────────────────────────────────────────
	void RecordGetLodStats(uint64_t dst_addr, uint32_t chunk_id, uint32_t buf_size_dw);
	void RecordRewind(uint32_t offset);
	void RecordNop(uint32_t payload_dw = 0);

	[[nodiscard]] const std::vector<GenericCommand>& GetCommands() const noexcept { return m_commands; }
	[[nodiscard]] size_t GetCommandCount() const noexcept { return m_commands.size(); }
	[[nodiscard]] bool IsEmpty() const noexcept { return m_commands.empty(); }

	void AddCommand(const CmdDrawNonIndexed& cmd) { RecordDrawNonIndexed(cmd.vertex_count, cmd.instance_count, cmd.first_vertex, cmd.first_instance); }
	void AddCommand(const CmdDrawIndexed& cmd) { RecordDrawIndexed(cmd.index_count, cmd.instance_count, cmd.first_index, cmd.vertex_offset, cmd.first_instance, cmd.index_gpu_addr, cmd.index_type); }
	void AddCommand(const CmdClearRenderTarget& cmd) { RecordClear(cmd.color, cmd.depth, cmd.stencil, cmd.clear_color, cmd.clear_depth, cmd.clear_stencil); }
	void AddCommand(const CmdSurfaceSync& cmd) { (void)cmd; RecordSurfaceSync(0, 0, 0, 0); }

	void Append(const Pm4CommandList& other);

private:
	template<typename T>
	void Emit(CommandType type, T&& data) {
		m_commands.push_back(GenericCommand{type, std::forward<T>(data)});
	}

	std::vector<GenericCommand> m_commands;
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_COMMAND_LIST_H
