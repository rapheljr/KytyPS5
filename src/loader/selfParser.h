// selfParser.h
//
// PS5 Signed ELF (SELF) Header & Container Parser.

#ifndef LOADER_SELF_PARSER_H
#define LOADER_SELF_PARSER_H

#include "common/common.h"

#include <cstdint>
#include <vector>

namespace Loader {

constexpr uint32_t kSelfMagic = 0x4F534C46; // "OSLF"

struct SelfHeader {
	uint32_t magic          = 0;
	uint8_t  version        = 0;
	uint8_t  mode           = 0;
	uint8_t  endian         = 0;
	uint8_t  attr           = 0;
	uint32_t key_type       = 0;
	uint16_t header_size    = 0;
	uint16_t meta_size      = 0;
	uint64_t file_size      = 0;
	uint16_t segment_count  = 0;
	uint16_t flags          = 0;
};

struct SelfSegmentHeader {
	uint64_t flags             = 0;
	uint64_t offset            = 0;
	uint64_t compressed_size   = 0;
	uint64_t uncompressed_size = 0;
};

struct SelfInfo {
	SelfHeader                    header;
	std::vector<SelfSegmentHeader> segments;
	bool                          valid = false;
	size_t                        elf_offset = 0;
	size_t                        elf_size   = 0;
};

class SelfParser {
public:
	SelfParser() = default;
	~SelfParser() = default;

	KYTY_CLASS_NO_COPY(SelfParser);

	static bool IsSelfBuffer(const uint8_t* buffer, size_t size);
	static bool Parse(const uint8_t* buffer, size_t size, SelfInfo& out_info);
	static bool ExtractElf(const uint8_t* self_buffer, size_t self_size, std::vector<uint8_t>& out_elf_buffer);
};

} // namespace Loader

#endif // LOADER_SELF_PARSER_H
