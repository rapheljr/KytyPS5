// ps5SaveDataSealedKeystore.h
//
// PS5 Sealed Keystore & Save Data Cryptographic Subsystem for KytyPS5.
// Manages title/user sealedkey files, key derivation (TitleID + AccountID), and HMAC integrity tags.

#ifndef LOADER_PS5_SAVE_DATA_SEALED_KEYSTORE_H
#define LOADER_PS5_SAVE_DATA_SEALED_KEYSTORE_H

#include "common/common.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Loader {

#pragma pack(push, 1)
struct SealedKeyHeader {
	uint32_t magic;          // 0x5345414C ("SEAL")
	uint32_t version;        // 1
	uint64_t account_id;
	char     title_id[16];
	uint8_t  hmac_tag[32];
	uint8_t  encrypted_key[32];
};
#pragma pack(pop)

struct KeystoreStats {
	uint64_t total_keys_sealed   = 0;
	uint64_t total_keys_unsealed = 0;
	uint32_t integrity_failures  = 0;
};

class Ps5SaveDataSealedKeystore {
public:
	Ps5SaveDataSealedKeystore();
	~Ps5SaveDataSealedKeystore() = default;

	KYTY_CLASS_NO_COPY(Ps5SaveDataSealedKeystore);

	/// Derive master key for a specific Title ID and User Account ID
	std::vector<uint8_t> DeriveKey(const std::string& title_id, uint64_t account_id);

	/// Seal (encrypt & sign) a raw save data key into a sealedkey payload
	std::vector<uint8_t> SealKey(const std::string& title_id, uint64_t account_id, const uint8_t* raw_key, size_t key_len);

	/// Unseal (verify & decrypt) a sealedkey payload into the raw save data key
	bool UnsealKey(const uint8_t* sealed_data, size_t sealed_size, const std::string& title_id, uint64_t account_id, std::vector<uint8_t>& out_key);

	/// Encrypt/Decrypt save data payload in-place
	bool TransformSaveData(uint8_t* data, size_t size, const std::vector<uint8_t>& key);

	[[nodiscard]] const KeystoreStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = {}; }

private:
	KeystoreStats m_stats{};
};

} // namespace Loader

#endif // LOADER_PS5_SAVE_DATA_SEALED_KEYSTORE_H
