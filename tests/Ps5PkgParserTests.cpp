// Ps5PkgParserTests.cpp
//
// Unit & Integration Tests for PS5 PKG Parser & Encrypted PFS Partition Mount Engine.

#include "kernel/ps5PkgParser.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace Libs::Kernel::Ps5;

static void TestPkgParserParseAndIntegrity() {
	std::printf("[TEST] PkgParserParseAndIntegrity\n");

	std::vector<uint8_t> mock_pkg(128 + 32);
	uint32_t magic = kPkgMagicPKG;
	uint32_t type = 1; // App
	uint32_t entry_count = 1;
	uint64_t table_off = 128;
	uint64_t body_off = 256;
	uint64_t body_sz = 1024;

	std::memcpy(mock_pkg.data(), &magic, 4);
	std::memcpy(mock_pkg.data() + 4, &type, 4);
	std::memcpy(mock_pkg.data() + 8, &entry_count, 4);
	std::memcpy(mock_pkg.data() + 12, &table_off, 8);
	std::memcpy(mock_pkg.data() + 20, &body_off, 8);
	std::memcpy(mock_pkg.data() + 28, &body_sz, 8);
	std::snprintf(reinterpret_cast<char*>(mock_pkg.data() + 36), 37, "HP0700-PPSA01234_00-GAME000000000000");

	// Add entry
	uint32_t entry_id = static_cast<uint32_t>(PkgEntryId::ParamSfo);
	uint64_t entry_off = 256;
	uint64_t entry_sz = 64;
	uint32_t flags = 0;
	std::memcpy(mock_pkg.data() + 128, &entry_id, 4);
	std::memcpy(mock_pkg.data() + 128 + 4, &entry_off, 8);
	std::memcpy(mock_pkg.data() + 128 + 12, &entry_sz, 8);
	std::memcpy(mock_pkg.data() + 128 + 20, &flags, 4);

	PkgParser parser;
	if (!parser.Parse(mock_pkg.data(), mock_pkg.size())) {
		std::fprintf(stderr, "FAIL: PkgParser::Parse returned false\n");
		std::exit(1);
	}

	if (!parser.VerifyIntegrity(mock_pkg.data(), mock_pkg.size())) {
		std::fprintf(stderr, "FAIL: PkgParser::VerifyIntegrity returned false\n");
		std::exit(1);
	}

	if (!parser.HasEntry(entry_id) || parser.GetHeader().magic != kPkgMagicPKG) {
		std::fprintf(stderr, "FAIL: PkgParser entry lookup failed\n");
		std::exit(1);
	}

	std::printf("  [ OK ] PkgParserParseAndIntegrity\n");
}

static void TestMountPfsImage() {
	std::printf("[TEST] PkgParserMountPfsImage\n");

	std::vector<uint8_t> mock_pfs(4096);
	uint32_t pfs_magic = 0x50465300; // "PFS\0"
	std::memcpy(mock_pfs.data(), &pfs_magic, 4);

	PfsMountInfo info;
	uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};

	if (!PkgParser::MountPfsImage(mock_pfs.data(), mock_pfs.size(), key, sizeof(key), info)) {
		std::fprintf(stderr, "FAIL: PkgParser::MountPfsImage returned false\n");
		std::exit(1);
	}

	if (!info.mounted || info.magic != 0x50465300 || info.block_size != 4096) {
		std::fprintf(stderr, "FAIL: PfsMountInfo parameter mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] PkgParserMountPfsImage\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PKG Container & PFS Partition Mount Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestPkgParserParseAndIntegrity();
	TestMountPfsImage();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
