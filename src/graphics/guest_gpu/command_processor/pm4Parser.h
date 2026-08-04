// pm4Parser.h
//
// Zero-copy PS5 PM4 ring buffer parser & packet decoder for Phase K.

#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_PARSER_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_PARSER_H

#include "common/common.h"
#include "graphics/guest_gpu/pm4.h"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace Libs::Graphics::Pm4 {

enum class PacketHeaderType : uint8_t {
	Type0 = 0,
	Type1 = 1,
	Type2 = 2,
	Type3 = 3,
	Invalid = 0xFF
};

struct PacketHeader {
	PacketHeaderType type     = PacketHeaderType::Invalid;
	uint8_t          opcode   = 0;
	uint16_t         count_dw = 0; // Total length in dwords including header
	uint32_t         raw      = 0;

	[[nodiscard]] bool IsType3() const noexcept { return type == PacketHeaderType::Type3; }
};

// Decoded PM4 Packet Parameter Structures

struct DrawPacket {
	uint32_t vertex_count    = 0;
	uint32_t instance_count  = 1;
	uint32_t first_vertex    = 0;
	uint32_t first_instance  = 0;
	uint32_t index_count     = 0;
	uint64_t index_gpu_addr  = 0;
	uint32_t index_type      = 0; // 0: 16-bit, 1: 32-bit
	bool     indexed         = false;
	bool     indirect        = false;
	uint64_t indirect_args   = 0;
};

struct DispatchPacket {
	uint32_t group_x        = 1;
	uint32_t group_y        = 1;
	uint32_t group_z        = 1;
	bool     indirect       = false;
	uint64_t indirect_args  = 0;
};

struct DmaCopyPacket {
	uint64_t src_addr       = 0;
	uint64_t dst_addr       = 0;
	uint32_t num_bytes      = 0;
	uint8_t  src_sel        = 0;
	uint8_t  dst_sel        = 0;
	bool     wait_completion = true;
};

struct ClearPacket {
	float    color[4]       = {0.0f, 0.0f, 0.0f, 1.0f};
	float    depth          = 1.0f;
	uint8_t  stencil        = 0;
	bool     clear_color    = true;
	bool     clear_depth    = false;
	bool     clear_stencil  = false;
};

struct BarrierPacket {
	uint32_t src_stage_mask = 0;
	uint32_t dst_stage_mask = 0;
	uint32_t src_access_mask = 0;
	uint32_t dst_access_mask = 0;
	bool     flush_inv_cb   = false;
	bool     flush_inv_db   = false;
};

struct EventPacket {
	uint32_t event_type     = 0;
	uint32_t event_index    = 0;
	uint64_t dst_gpu_addr   = 0;
	uint64_t value          = 0;
	bool     write_eop      = false;
};

struct QueryPacket {
	uint64_t query_gpu_addr = 0;
	uint32_t query_type     = 0; // 0: Timestamp, 1: Occlusion
};

struct SetRegisterPacket {
	uint32_t reg_offset     = 0;
	uint32_t count          = 0;
	const uint32_t* values  = nullptr;
	bool     is_sh_reg      = false;
	bool     is_context_reg = false;
};

using DecodedPacketData = std::variant<
	DrawPacket,
	DispatchPacket,
	DmaCopyPacket,
	ClearPacket,
	BarrierPacket,
	EventPacket,
	QueryPacket,
	SetRegisterPacket
>;

struct DecodedPacket {
	PacketHeader      header;
	uint32_t          opcode = 0;
	const uint32_t*   payload = nullptr;
	uint32_t          payload_dw = 0;
	DecodedPacketData data;
};

// PM4 Ring Buffer Parser Error Policy
enum class ParserErrorAction {
	SkipPacket,
	AbortStream,
	RecoverNextHeader
};

struct ParserStats {
	uint64_t packets_parsed     = 0;
	uint64_t dwords_parsed      = 0;
	uint64_t draw_packets       = 0;
	uint64_t dispatch_packets   = 0;
	uint64_t dma_packets        = 0;
	uint64_t barrier_packets    = 0;
	uint64_t indirect_buffers   = 0;
	uint64_t parse_errors       = 0;
};

class Pm4RingBufferParser {
public:
	Pm4RingBufferParser() = default;
	~Pm4RingBufferParser() = default;

	KYTY_CLASS_NO_COPY(Pm4RingBufferParser);

	static PacketHeader DecodeHeader(uint32_t header_dw) noexcept;

	using PacketCallback = std::function<void(const DecodedPacket& packet)>;
	using ErrorCallback  = std::function<ParserErrorAction(uint32_t raw_header, size_t offset_dw, const char* reason)>;

	void SetPacketCallback(PacketCallback callback) { m_packet_callback = std::move(callback); }
	void SetErrorCallback(ErrorCallback callback) { m_error_callback = std::move(callback); }

	bool ParseStream(const uint32_t* stream_ptr, size_t size_dw, uint32_t max_depth = 8);

	[[nodiscard]] const ParserStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = ParserStats{}; }

private:
	bool ParsePacketInternal(const uint32_t* ptr, size_t remaining_dw, size_t& out_consumed_dw, uint32_t current_depth, uint32_t max_depth);
	DecodedPacketData DecodePayload(const PacketHeader& header, const uint32_t* payload, uint32_t payload_dw);

	PacketCallback m_packet_callback;
	ErrorCallback  m_error_callback;
	ParserStats    m_stats;
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_PARSER_H
