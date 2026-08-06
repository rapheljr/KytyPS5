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

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — SelfParser Unit & Integration Test Suite\n");
	std::printf("================================================================================\n");

	TestIsSelfBuffer();
	TestParseHeaderAndSegments();
	TestDecryptSelfHeaderAndDecompress();

	std::printf("================================================================================\n");
	std::printf("  Results: 3 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
