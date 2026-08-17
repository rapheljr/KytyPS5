// Ps5PfsMountTests.cpp
//
// Unit & Integration tests for PlayStation File System (PFS) Container Parsing,
// Decryption, and File Inode Extraction.

#include "loader/ps5PfsDecryptor.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader;

void TestPfsSuperblockValidation() {
	std::printf("[TEST] PFS Superblock Validation...\n");

	Ps5PfsDecryptor decryptor;
	Check(!decryptor.IsValid(), "Initial decryptor state should be invalid");

	std::string test_data = "PS5_TEST_ELF_IMAGE_CONTENT_XYZ";
	auto mock_img = Ps5PfsDecryptor::CreateMockPfsImage("eboot.bin", test_data);

	bool loaded = decryptor.LoadFromMemory(mock_img.data(), mock_img.size());
	Check(loaded, "Failed to load mock PFS image");
	Check(decryptor.IsValid(), "Decryptor should be valid");
	Check(decryptor.GetSuperBlock().magic == PFS_SUPERBLOCK_MAGIC_A, "Magic mismatch");
	Check(decryptor.GetEntries().size() == 1, "Expected 1 entry");
	Check(decryptor.GetEntries()[0].name == "eboot.bin", "Entry name mismatch");

	std::printf("  [OK] PFS Superblock Validation\n");
}

void TestMultiFilePfsExtraction() {
	std::printf("[TEST] PFS Multi-File Inode Resolution & Extraction...\n");

	std::vector<std::pair<std::string, std::string>> files = {
		{"eboot.bin", "MAIN_APPLICATION_EXECUTABLE_BINARY"},
		{"content_file_1.bin", "TEXTURE_STREAM_PACKAGE_DATA"},
		{"content_file_2.bin", "AUDIO_WAVE_DATA_BANK"}
	};

	auto mock_pfs = Ps5PfsDecryptor::CreateMultiFileMockPfsImage(files);

	Ps5PfsDecryptor decryptor;
	bool loaded = decryptor.LoadFromMemory(mock_pfs.data(), mock_pfs.size());
	Check(loaded, "Failed to load multi-file PFS");
	Check(decryptor.GetEntries().size() == 3, "Expected 3 PFS entries");

	// Extract files
	std::vector<uint8_t> out1, out2, out3;
	bool ext1 = decryptor.ExtractFile(mock_pfs.data(), mock_pfs.size(), "eboot.bin", out1);
	bool ext2 = decryptor.ExtractFile(mock_pfs.data(), mock_pfs.size(), "content_file_1.bin", out2);
	bool ext3 = decryptor.ExtractFile(mock_pfs.data(), mock_pfs.size(), "content_file_2.bin", out3);

	Check(ext1 && std::string(out1.begin(), out1.end()) == "MAIN_APPLICATION_EXECUTABLE_BINARY", "Extract file 1 failed");
	Check(ext2 && std::string(out2.begin(), out2.end()) == "TEXTURE_STREAM_PACKAGE_DATA", "Extract file 2 failed");
	Check(ext3 && std::string(out3.begin(), out3.end()) == "AUDIO_WAVE_DATA_BANK", "Extract file 3 failed");

	std::printf("  [OK] PFS Multi-File Inode Resolution & Extraction\n");
}

void TestPfsSectorDecryption() {
	std::printf("[TEST] PFS Block Sector In-Place Decryption...\n");

	Ps5PfsDecryptor decryptor;
	decryptor.SetPasscodeKey("00000000000000000000000000000000");

	std::vector<uint8_t> block(4096, 0xAA);
	bool dec = decryptor.DecryptBlock(block.data(), block.size(), 12);
	Check(dec, "Sector decryption failed");
	Check(block[0] != 0xAA, "Block data should be modified after decryption");

	std::printf("  [OK] PFS Block Sector In-Place Decryption\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PlayStation File System (PFS) Container Test Suite\n");
	std::printf("================================================================================\n");

	TestPfsSuperblockValidation();
	TestMultiFilePfsExtraction();
	TestPfsSectorDecryption();

	std::printf("================================================================================\n");
	std::printf("  Results: All PFS Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
