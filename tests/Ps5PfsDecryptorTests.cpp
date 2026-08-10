// Ps5PfsDecryptorTests.cpp
//
// Unit & Integration Tests for PS5 PFS Container Decryptor and Inode Parser.

#include "loader/ps5PfsDecryptor.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Loader;

static void TestPfsSuperBlockValidation() {
	std::printf("[TEST] PfsSuperBlock_Validation\n");

	Ps5PfsDecryptor decryptor;

	// Invalid buffer
	std::vector<uint8_t> invalid_buf(128, 0);
	if (decryptor.LoadFromMemory(invalid_buf.data(), invalid_buf.size())) {
		std::fprintf(stderr, "FAIL: LoadFromMemory succeeded on invalid buffer\n");
		std::exit(1);
	}

	// Valid synthesized PFS image
	std::string test_payload = "\x7f\x45\x4c\x46\x02\x01\x01\x09MockELFPayloadContent";
	auto valid_image = Ps5PfsDecryptor::CreateMockPfsImage("eboot.bin", test_payload);

	if (!decryptor.LoadFromMemory(valid_image.data(), valid_image.size())) {
		std::fprintf(stderr, "FAIL: LoadFromMemory failed on valid mock PFS image\n");
		std::exit(1);
	}

	if (!decryptor.IsValid()) {
		std::fprintf(stderr, "FAIL: IsValid returned false on valid PFS image\n");
		std::exit(1);
	}

	const auto& sb = decryptor.GetSuperBlock();
	if (sb.magic != PFS_SUPERBLOCK_MAGIC_A || sb.block_size != 4096) {
		std::fprintf(stderr, "FAIL: SuperBlock magic or block size mismatch\n");
		std::exit(1);
	}

	const auto& entries = decryptor.GetEntries();
	if (entries.empty() || entries[0].name != "eboot.bin" || entries[0].size != test_payload.size()) {
		std::fprintf(stderr, "FAIL: Extracted entry info mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] PfsSuperBlock_Validation\n");
}

static void TestPfsSectorDecryption() {
	std::printf("[TEST] PfsSector_Decryption\n");

	Ps5PfsDecryptor decryptor;
	decryptor.SetPasscodeKey("0123456789abcdef0123456789abcdef");

	std::vector<uint8_t> block(4096, 0xAA);
	std::vector<uint8_t> original_block = block;

	// Decrypt sector 42
	bool ok = decryptor.DecryptBlock(block.data(), block.size(), 42);
	if (!ok) {
		std::fprintf(stderr, "FAIL: DecryptBlock failed\n");
		std::exit(1);
	}

	// Ciphertext must differ from original plaintext
	if (block == original_block) {
		std::fprintf(stderr, "FAIL: DecryptBlock did not mutate ciphertext\n");
		std::exit(1);
	}

	// Re-decrypting the same block with same key should revert (symmetric XOR/XTS mock)
	decryptor.DecryptBlock(block.data(), block.size(), 42);
	if (block != original_block) {
		std::fprintf(stderr, "FAIL: Invertible sector decryption failed to restore original content\n");
		std::exit(1);
	}

	std::printf("  [ OK ] PfsSector_Decryption\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 PFS Container Decryptor Test Suite\n");
	std::printf("================================================================================\n");

	TestPfsSuperBlockValidation();
	TestPfsSectorDecryption();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
