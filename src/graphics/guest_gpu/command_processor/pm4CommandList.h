// pm4CommandList.h
//
// Backend-independent GPU command list data structure for Phase K.

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
	DispatchDirect,
	DispatchIndirect,
	DmaCopy,
	ClearRenderTarget,
	PipelineBarrier,
	SetEvent,
	TimestampQuery,
	SetRegisterState,
	SetPredication,
	MemSemaphore,
	SetIndexType
};

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
	uint32_t index_type     = 0; // 0: uint16, 1: uint32
};

struct CmdDrawIndirect {
	uint64_t args_gpu_addr  = 0;
	uint32_t draw_count     = 1;
	uint32_t stride_bytes   = 0;
	bool     indexed        = false;
};

struct CmdDispatchDirect {
	uint32_t group_count_x  = 1;
	uint32_t group_count_y  = 1;
	uint32_t group_count_z  = 1;
};

struct CmdDispatchIndirect {
	uint64_t args_gpu_addr  = 0;
};

struct CmdDmaCopy {
	uint64_t src_gpu_addr   = 0;
	uint64_t dst_gpu_addr   = 0;
	uint32_t num_bytes      = 0;
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
	bool     flush_cb       = true;
	bool     flush_db       = true;
	bool     inv_l2         = true;
};

struct CmdSetEvent {
	uint32_t event_type     = 0;
	uint64_t signal_address = 0;
	uint64_t value          = 0;
};

struct CmdTimestampQuery {
	uint64_t query_address  = 0;
};

struct CmdSetRegisterState {
	uint32_t reg_offset     = 0;
	uint32_t count          = 0;
	uint32_t values[16]     = {0};
	bool     is_sh_reg      = false;
};

struct CmdSetPredication {
	uint64_t query_gpu_addr = 0;
	bool     pred_enable    = false;
	bool     hint_draw      = false;
};

struct CmdMemSemaphore {
	uint64_t sem_gpu_addr   = 0;
	uint32_t sem_op         = 0;
};

struct CmdSetIndexType {
	uint32_t index_type     = 0;
};

using GenericCommandData = std::variant<
	CmdDrawNonIndexed,
	CmdDrawIndexed,
	CmdDrawIndirect,
	CmdDispatchDirect,
	CmdDispatchIndirect,
	CmdDmaCopy,
	CmdClearRenderTarget,
	CmdPipelineBarrier,
	CmdSetEvent,
	CmdTimestampQuery,
	CmdSetRegisterState,
	CmdSetPredication,
	CmdMemSemaphore,
	CmdSetIndexType
>;


struct GenericCommand {
	CommandType        type = CommandType::DrawNonIndexed;
	GenericCommandData data;
};

class Pm4CommandList {
public:
	Pm4CommandList() = default;
	~Pm4CommandList() = default;

	KYTY_CLASS_NO_COPY(Pm4CommandList);

	void Reserve(size_t capacity) { m_commands.reserve(capacity); }
	void Clear() noexcept { m_commands.clear(); }

	void RecordDrawNonIndexed(uint32_t vertex_count, uint32_t instance_count = 1, uint32_t first_vertex = 0, uint32_t first_instance = 0);
	void RecordDrawIndexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, int32_t vertex_offset, uint32_t first_instance, uint64_t index_addr, uint32_t index_type);
	void RecordDrawIndirect(uint64_t args_addr, uint32_t draw_count, uint32_t stride_bytes, bool indexed);
	void RecordDispatch(uint32_t group_x, uint32_t group_y, uint32_t group_z);
	void RecordDispatchIndirect(uint64_t args_addr);
	void RecordDmaCopy(uint64_t src_addr, uint64_t dst_addr, uint32_t num_bytes);
	void RecordClear(const float color[4], float depth, uint8_t stencil, bool clear_color, bool clear_depth, bool clear_stencil);
	void RecordBarrier(bool flush_cb, bool flush_db, bool inv_l2);
	void RecordSetEvent(uint32_t event_type, uint64_t signal_addr, uint64_t value);
	void RecordTimestampQuery(uint64_t query_addr);
	void RecordSetRegister(uint32_t reg_offset, const uint32_t* values, uint32_t count, bool is_sh);
	void RecordSetPredication(uint64_t query_addr, bool enable, bool hint_draw);
	void RecordMemSemaphore(uint64_t sem_addr, uint32_t op);
	void RecordSetIndexType(uint32_t index_type);


	[[nodiscard]] const std::vector<GenericCommand>& GetCommands() const noexcept { return m_commands; }
	[[nodiscard]] size_t GetCommandCount() const noexcept { return m_commands.size(); }
	[[nodiscard]] bool IsEmpty() const noexcept { return m_commands.empty(); }

	void Append(const Pm4CommandList& other);

private:
	std::vector<GenericCommand> m_commands;
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_COMMAND_LIST_H
