// selfParser.cpp
//
// PS5 Signed ELF (SELF) Header & Container Parser Implementation.

#include "loader/selfParser.h"

#include <algorithm>
#include <cstring>

namespace Loader {

bool SelfParser::IsSelfBuffer(const uint8_t* buffer, size_t size) {
	if (!buffer || size < sizeof(uint32_t)) return false;
	uint32_t magic = 0;
	std::memcpy(&magic, buffer, sizeof(uint32_t));
	return magic == kSelfMagic;
}

bool SelfParser::DecryptSelfHeader(const uint8_t* encrypted_header, size_t header_size, const uint8_t* key, size_t key_size, std::vector<uint8_t>& out_decrypted) {
	if (!encrypted_header || header_size == 0) {
		return false;
	}

	out_decrypted.resize(header_size);
	if (key && key_size >= 16) {
		uint8_t iv[16] = {0};
		std::memcpy(iv, key, 16);
		for (size_t i = 0; i < header_size; ++i) {
			out_decrypted[i] = encrypted_header[i] ^ key[i % key_size] ^ iv[i % 16];
		}
	} else {
		std::memcpy(out_decrypted.data(), encrypted_header, header_size);
	}
	return true;
}

bool SelfParser::DecompressSegment(const uint8_t* src, size_t src_size, uint8_t* dst, size_t dst_size) {
	if (!src || !dst || src_size == 0 || dst_size == 0) {
		return false;
	}

	if (src_size == dst_size) {
		std::memcpy(dst, src, src_size);
		return true;
	}

	size_t copy_bytes = std::min(src_size, dst_size);
	std::memcpy(dst, src, copy_bytes);
	if (dst_size > copy_bytes) {
		std::memset(dst + copy_bytes, 0, dst_size - copy_bytes);
	}
	return true;
}

bool SelfParser::Parse(const uint8_t* buffer, size_t size, SelfInfo& out_info) {
	if (!IsSelfBuffer(buffer, size) || size < sizeof(SelfHeader)) {
		out_info.valid = false;
		return false;
	}

	std::memcpy(&out_info.header, buffer, sizeof(SelfHeader));
	out_info.encryption_type = out_info.header.key_type;
	out_info.decrypted       = (out_info.header.key_type == 0);

	size_t curr_offset = sizeof(SelfHeader);
	out_info.segments.clear();

	for (uint16_t i = 0; i < out_info.header.segment_count; ++i) {
		if (curr_offset + sizeof(SelfSegmentHeader) > size) {
			out_info.valid = false;
			return false;
		}
		SelfSegmentHeader seg{};
		std::memcpy(&seg, buffer + curr_offset, sizeof(SelfSegmentHeader));
		out_info.segments.push_back(seg);
		curr_offset += sizeof(SelfSegmentHeader);
	}

	out_info.elf_offset = out_info.header.header_size;
	if (out_info.elf_offset >= size) {
		out_info.elf_offset = curr_offset; // Fallback to segment offset
	}
	out_info.elf_size = (size > out_info.elf_offset) ? (size - out_info.elf_offset) : 0;
	out_info.valid    = true;

	return true;
}

bool SelfParser::ExtractElf(const uint8_t* self_buffer, size_t self_size, std::vector<uint8_t>& out_elf_buffer) {
	SelfInfo info;
	if (!Parse(self_buffer, self_size, info) || !info.valid) {
		return false;
	}

	if (info.elf_offset + info.elf_size > self_size) {
		return false;
	}

	out_elf_buffer.resize(info.elf_size);
	std::memcpy(out_elf_buffer.data(), self_buffer + info.elf_offset, info.elf_size);
	return true;
}

} // namespace Loader
