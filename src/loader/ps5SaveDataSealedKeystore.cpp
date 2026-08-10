// ps5SaveDataSealedKeystore.cpp
//
// PS5 Sealed Keystore & Save Data Cryptographic Subsystem Implementation.

#include "loader/ps5SaveDataSealedKeystore.h"

#include <cstring>

namespace Loader {

Ps5SaveDataSealedKeystore::Ps5SaveDataSealedKeystore() = default;

std::vector<uint8_t> Ps5SaveDataSealedKeystore::DeriveKey(const std::string& title_id, uint64_t account_id) {
	std::vector<uint8_t> derived(32, 0);

	// Seed with Title ID and Account ID
	for (size_t i = 0; i < 32; ++i) {
		uint8_t t_byte = i < title_id.size() ? static_cast<uint8_t>(title_id[i]) : 0x55;
		uint8_t a_byte = static_cast<uint8_t>((account_id >> ((i % 8) * 8)) & 0xFF);
		derived[i] = t_byte ^ a_byte ^ static_cast<uint8_t>(i * 7 + 0x3A);
	}

	return derived;
}

std::vector<uint8_t> Ps5SaveDataSealedKeystore::SealKey(const std::string& title_id, uint64_t account_id, const uint8_t* raw_key, size_t key_len) {
	if (!raw_key || key_len == 0) return {};

	SealedKeyHeader header{};
	header.magic      = 0x5345414C; // "SEAL"
	header.version    = 1;
	header.account_id = account_id;
	std::strncpy(header.title_id, title_id.c_str(), sizeof(header.title_id) - 1);

	auto master_key = DeriveKey(title_id, account_id);

	// Encrypt key
	for (size_t i = 0; i < 32; ++i) {
		uint8_t k = (i < key_len) ? raw_key[i] : 0;
		header.encrypted_key[i] = k ^ master_key[i];
	}

	// Generate HMAC tag
	for (size_t i = 0; i < 32; ++i) {
		header.hmac_tag[i] = header.encrypted_key[i] ^ master_key[31 - i] ^ 0xAA;
	}

	std::vector<uint8_t> output(sizeof(SealedKeyHeader));
	std::memcpy(output.data(), &header, sizeof(SealedKeyHeader));

	m_stats.total_keys_sealed++;
	return output;
}

bool Ps5SaveDataSealedKeystore::UnsealKey(const uint8_t* sealed_data, size_t sealed_size, const std::string& title_id, uint64_t account_id, std::vector<uint8_t>& out_key) {
	if (!sealed_data || sealed_size < sizeof(SealedKeyHeader)) {
		m_stats.integrity_failures++;
		return false;
	}

	SealedKeyHeader header{};
	std::memcpy(&header, sealed_data, sizeof(SealedKeyHeader));

	if (header.magic != 0x5345414C || header.account_id != account_id ||
	    std::strncmp(header.title_id, title_id.c_str(), sizeof(header.title_id) - 1) != 0) {
		m_stats.integrity_failures++;
		return false;
	}

	auto master_key = DeriveKey(title_id, account_id);

	// Verify HMAC tag
	for (size_t i = 0; i < 32; ++i) {
		uint8_t expected_tag = header.encrypted_key[i] ^ master_key[31 - i] ^ 0xAA;
		if (header.hmac_tag[i] != expected_tag) {
			m_stats.integrity_failures++;
			return false;
		}
	}

	out_key.resize(32);
	for (size_t i = 0; i < 32; ++i) {
		out_key[i] = header.encrypted_key[i] ^ master_key[i];
	}

	m_stats.total_keys_unsealed++;
	return true;
}

bool Ps5SaveDataSealedKeystore::TransformSaveData(uint8_t* data, size_t size, const std::vector<uint8_t>& key) {
	if (!data || size == 0 || key.empty()) return false;

	for (size_t i = 0; i < size; ++i) {
		data[i] ^= key[i % key.size()];
	}

	return true;
}

} // namespace Loader
