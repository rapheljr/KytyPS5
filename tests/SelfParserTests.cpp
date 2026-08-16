// SelfParserTests.cpp
//
// Integration & Unit Tests for PS5 SELF Header Parser & Decrypter Interface.

#include "loader/selfParser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void TestIsSelfBuffer() {
	std::printf("[TEST] SelfParserIsSelfBuffer\n");

	uint8_t invalid_buf[8] = {0x00, 0x00, 0x00, 0x00};
	if (Loader::SelfParser::IsSelfBuffer(invalid_buf, sizeof(invalid_buf))) {
		std::fprintf(stderr, "FAIL: IsSelfBuffer returned true for invalid magic\n");
		std::exit(1);
	}

	uint8_t valid_self[8] = {0x46, 0x4C, 0x53, 0x4F, 0x00, 0x00, 0x00, 0x00}; // "OSLF"
	if (!Loader::SelfParser::IsSelfBuffer(valid_self, sizeof(valid_self))) {
		std::fprintf(stderr, "FAIL: IsSelfBuffer returned false for valid kSelfMagic\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SelfParserIsSelfBuffer\n");
}

static void TestParseHeaderAndSegments() {
	std::printf("[TEST] SelfParserParseHeaderAndSegments\n");

	std::vector<uint8_t> buffer(sizeof(Loader::SelfHeader) + sizeof(Loader::SelfSegmentHeader));
	auto* hdr = reinterpret_cast<Loader::SelfHeader*>(buffer.data());
	hdr->magic = Loader::kSelfMagic;
	hdr->version = 1;
	hdr->key_type = 0;
	hdr->segment_count = 1;
	hdr->header_size = static_cast<uint16_t>(buffer.size());

	auto* seg = reinterpret_cast<Loader::SelfSegmentHeader*>(buffer.data() + sizeof(Loader::SelfHeader));
	seg->flags = 0x1;
	seg->offset = sizeof(Loader::SelfHeader) + sizeof(Loader::SelfSegmentHeader);
	seg->compressed_size = 64;
	seg->uncompressed_size = 64;

	Loader::SelfInfo info;
	if (!Loader::SelfParser::Parse(buffer.data(), buffer.size(), info) || !info.valid) {
		std::fprintf(stderr, "FAIL: Parse failed for valid mock SELF buffer\n");
		std::exit(1);
	}

	if (info.header.magic != Loader::kSelfMagic || info.segments.size() != 1 || !info.decrypted) {
		std::fprintf(stderr, "FAIL: Parse self info parameters mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SelfParserParseHeaderAndSegments\n");
}

static void TestDecryptSelfHeaderAndDecompress() {
	std::printf("[TEST] SelfParserDecryptSelfHeaderAndDecompress\n");

	uint8_t enc_header[32];
	for (size_t i = 0; i < sizeof(enc_header); ++i) {
		enc_header[i] = static_cast<uint8_t>(i ^ 0xAA);
	}

	uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
	std::vector<uint8_t> dec_header;

	if (!Loader::SelfParser::DecryptSelfHeader(enc_header, sizeof(enc_header), key, sizeof(key), dec_header)) {
		std::fprintf(stderr, "FAIL: DecryptSelfHeader failed\n");
		std::exit(1);
	}

	if (dec_header.size() != sizeof(enc_header)) {
		std::fprintf(stderr, "FAIL: DecryptSelfHeader output size mismatch\n");
		std::exit(1);
	}

	uint8_t compressed[16] = {0x11, 0x22, 0x33, 0x44};
	uint8_t decompressed[16] = {0};

	if (!Loader::SelfParser::DecompressSegment(compressed, 4, decompressed, 16)) {
		std::fprintf(stderr, "FAIL: DecompressSegment failed\n");
		std::exit(1);
	}

	if (std::memcmp(decompressed, compressed, 4) != 0) {
		std::fprintf(stderr, "FAIL: DecompressSegment data copy mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SelfParserDecryptSelfHeaderAndDecompress\n");
}

static void TestExtractElfMultiSegment() {
	std::printf("[TEST] SelfParserExtractElfMultiSegment\n");

	// Construct a multi-segment mock SELF
	// Segment 0: ELF Header (uncompressed, offset 0 in ELF, size 64)
	// Segment 1: Program Code (uncompressed/direct copy, offset 64 in ELF, size 128)
	size_t self_hdr_size = sizeof(Loader::SelfHeader) + 2 * sizeof(Loader::SelfSegmentHeader);
	std::vector<uint8_t> buffer(self_hdr_size + 64 + 128, 0);

	auto* hdr = reinterpret_cast<Loader::SelfHeader*>(buffer.data());
	hdr->magic = Loader::kSelfMagic;
	hdr->version = 1;
	hdr->key_type = 0;
	hdr->segment_count = 2;
	hdr->header_size = static_cast<uint16_t>(self_hdr_size);

	auto* segs = reinterpret_cast<Loader::SelfSegmentHeader*>(buffer.data() + sizeof(Loader::SelfHeader));

	// Segment 0
	segs[0].flags = 0x1;
	segs[0].offset = self_hdr_size;
	segs[0].compressed_size = 64;
	segs[0].uncompressed_size = 64;
	for (size_t i = 0; i < 64; ++i) {
		buffer[self_hdr_size + i] = static_cast<uint8_t>(0x7F + i);
	}

	// Segment 1
	segs[1].flags = 0x1;
	segs[1].offset = self_hdr_size + 64;
	segs[1].compressed_size = 128;
	segs[1].uncompressed_size = 128;
	for (size_t i = 0; i < 128; ++i) {
		buffer[self_hdr_size + 64 + i] = static_cast<uint8_t>(0xAA ^ i);
	}

	Loader::SelfInfo info;
	if (!Loader::SelfParser::Parse(buffer.data(), buffer.size(), info) || !info.valid) {
		std::fprintf(stderr, "FAIL: Parse failed for multi-segment SELF\n");
		std::exit(1);
	}

	std::vector<uint8_t> extracted_elf;
	if (!Loader::SelfParser::ExtractElf(buffer.data(), buffer.size(), extracted_elf)) {
		std::fprintf(stderr, "FAIL: ExtractElf failed for multi-segment SELF\n");
		std::exit(1);
	}

	if (extracted_elf.size() != 64 + 128) {
		std::fprintf(stderr, "FAIL: ExtractElf returned unexpected size %zu\n", extracted_elf.size());
		std::exit(1);
	}

	if (std::memcmp(extracted_elf.data(), buffer.data() + self_hdr_size, 64) != 0 ||
	    std::memcmp(extracted_elf.data() + 64, buffer.data() + self_hdr_size + 64, 128) != 0) {
		std::fprintf(stderr, "FAIL: ExtractElf content mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SelfParserExtractElfMultiSegment\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — SelfParser Unit & Integration Test Suite\n");
	std::printf("================================================================================\n");

	TestIsSelfBuffer();
	TestParseHeaderAndSegments();
	TestDecryptSelfHeaderAndDecompress();
	TestExtractElfMultiSegment();

	std::printf("================================================================================\n");
	std::printf("  Results: 4 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
