// Ps5SaveDataSealedKeystoreTests.cpp
//
// Unit & Integration Tests for PS5 Sealed Keystore & Save Data Subsystem.

#include "loader/ps5SaveDataSealedKeystore.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace Loader;

static void TestSealAndUnsealKey() {
	std::printf("[TEST] SealedKeystore_SealAndUnseal\n");

	Ps5SaveDataSealedKeystore keystore;
	std::string title_id = "CUSA12345";
	uint64_t account_id = 0x10000000ULL;

	std::vector<uint8_t> original_key = {
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
		0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
		0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20
	};

	// 1. Seal key
	auto sealed_data = keystore.SealKey(title_id, account_id, original_key.data(), original_key.size());
	if (sealed_data.empty()) {
		std::fprintf(stderr, "FAIL: SealKey returned empty payload\n");
		std::exit(1);
	}

	// 2. Unseal key with matching credentials
	std::vector<uint8_t> unsealed_key;
	bool ok = keystore.UnsealKey(sealed_data.data(), sealed_data.size(), title_id, account_id, unsealed_key);
	if (!ok || unsealed_key != original_key) {
		std::fprintf(stderr, "FAIL: Unsealed key did not match original key\n");
		std::exit(1);
	}

	// 3. Unseal key with mismatched Title ID (must fail)
	std::vector<uint8_t> fail_key;
	if (keystore.UnsealKey(sealed_data.data(), sealed_data.size(), "CUSA99999", account_id, fail_key)) {
		std::fprintf(stderr, "FAIL: UnsealKey succeeded with wrong title ID\n");
		std::exit(1);
	}

	const auto& stats = keystore.GetStats();
	if (stats.total_keys_sealed != 1 || stats.total_keys_unsealed != 1 || stats.integrity_failures != 1) {
		std::fprintf(stderr, "FAIL: Keystore stats mismatch (Sealed=%llu, Unsealed=%llu, Failures=%u)\n",
		             stats.total_keys_sealed, stats.total_keys_unsealed, stats.integrity_failures);
		std::exit(1);
	}

	std::printf("  [ OK ] SealedKeystore_SealAndUnseal\n");
}

static void TestSaveDataTransformation() {
	std::printf("[TEST] SealedKeystore_SaveDataTransformation\n");

	Ps5SaveDataSealedKeystore keystore;
	std::vector<uint8_t> key(32, 0x7F);

	std::string plain = "KytyPS5_SaveGameState_Checkpoint_Level5";
	std::vector<uint8_t> buffer(plain.begin(), plain.end());
	std::vector<uint8_t> original = buffer;

	// Encrypt
	keystore.TransformSaveData(buffer.data(), buffer.size(), key);
	if (buffer == original) {
		std::fprintf(stderr, "FAIL: Transform did not encrypt payload\n");
		std::exit(1);
	}

	// Decrypt
	keystore.TransformSaveData(buffer.data(), buffer.size(), key);
	if (buffer != original) {
		std::fprintf(stderr, "FAIL: Transform did not restore original payload\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SealedKeystore_SaveDataTransformation\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 Sealed Keystore & Save Data Test Suite\n");
	std::printf("================================================================================\n");

	TestSealAndUnsealKey();
	TestSaveDataTransformation();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
