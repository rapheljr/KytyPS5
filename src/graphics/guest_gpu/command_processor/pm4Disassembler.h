// pm4Disassembler.h
//
// PM4 packet disassembler, validation layer & command tracer for Phase K.

#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_DISASSEMBLER_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_DISASSEMBLER_H

#include "common/common.h"
#include "graphics/guest_gpu/command_processor/pm4Parser.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Libs::Graphics::Pm4 {

struct ValidationIssue {
	enum class Severity { Warning, Error, Critical };
	Severity    severity = Severity::Error;
	size_t      dword_offset = 0;
	uint32_t    opcode = 0;
	std::string message;
};

class Pm4Disassembler {
public:
	Pm4Disassembler() = default;
	~Pm4Disassembler() = default;

	KYTY_CLASS_NO_COPY(Pm4Disassembler);

	static std::string GetOpcodeName(uint32_t opcode) noexcept;
	static std::string DisassemblePacket(const DecodedPacket& packet);
	static std::string DisassembleStream(const uint32_t* stream_ptr, size_t size_dw);

	static std::vector<ValidationIssue> ValidateStream(const uint32_t* stream_ptr, size_t size_dw) noexcept;
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_DISASSEMBLER_H
