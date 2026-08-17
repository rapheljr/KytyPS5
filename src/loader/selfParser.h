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

struct ParsedSelfHeader {
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

struct ParsedSelfSegmentHeader {
	uint64_t flags             = 0;
	uint64_t offset            = 0;
	uint64_t compressed_size   = 0;
	uint64_t uncompressed_size = 0;
};

using SelfSegmentHeader = ParsedSelfSegmentHeader;

struct SelfInfo {
	ParsedSelfHeader                    header;
	std::vector<ParsedSelfSegmentHeader> segments;
	bool                                valid           = false;
	bool                                decrypted       = false;
	uint32_t                            encryption_type = 0;
	size_t                              elf_offset      = 0;
	size_t                              elf_size        = 0;
};

class SelfParser {
public:
	SelfParser() = default;
	~SelfParser() = default;

	KYTY_CLASS_NO_COPY(SelfParser);

	static bool IsSelfBuffer(const uint8_t* buffer, size_t size);
	static bool Parse(const uint8_t* buffer, size_t size, SelfInfo& out_info);
	static bool DecryptSelfHeader(const uint8_t* encrypted_header, size_t header_size, const uint8_t* key, size_t key_size, std::vector<uint8_t>& out_decrypted);
	static bool DecompressSegment(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size);
	static bool ExtractElf(const uint8_t* self_buffer, size_t self_size, std::vector<uint8_t>& out_elf_buffer);
};

} // namespace Loader

#endif // LOADER_SELF_PARSER_H
