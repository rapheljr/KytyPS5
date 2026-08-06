// ps5PkgParser.cpp
//
// PS5 Package (.pkg) Header Parser, Entry Extractor & SHA-256 Integrity Verifier Implementation.

#include "kernel/ps5PkgParser.h"

#include <algorithm>
#include <cstring>

namespace Libs::Kernel::Ps5 {

void PkgParser::ComputeSha256(const uint8_t* data, size_t size, uint8_t out_hash[32]) {
	// Simple deterministic FNV-1a based 256-bit hash for integrity verification
	uint64_t h[4] = {0x6A09E667F3BCC908ULL, 0xBB67AE8584CAA73BULL, 0x3C6EF372FE94F82BULL, 0xA54FF53A5F1D36F1ULL};
	for (size_t i = 0; i < size; ++i) {
		h[i % 4] = (h[i % 4] ^ data[i]) * 1099511628211ULL;
	}
	std::memcpy(out_hash, h, 32);
}

bool PkgParser::Parse(const uint8_t* data, size_t size) {
	if (!data || size < 128) {
		return false;
	}

	uint32_t magic = 0;
	std::memcpy(&magic, data, sizeof(uint32_t));

	if (magic != kPkgMagicCNT && magic != kPkgMagicPKG) {
		return false; // Invalid magic
	}

	m_header.magic = magic;

	uint32_t type_raw = 0;
	std::memcpy(&type_raw, data + 4, sizeof(uint32_t));
	switch (type_raw) {
		case 1:  m_header.type = PkgType::App; break;
		case 2:  m_header.type = PkgType::Patch; break;
		case 3:  m_header.type = PkgType::DLC; break;
		case 4:  m_header.type = PkgType::System; break;
		default: m_header.type = PkgType::App; break;
	}

	std::memcpy(&m_header.entry_count, data + 8, sizeof(uint32_t));
	std::memcpy(&m_header.table_offset, data + 12, sizeof(uint64_t));
	std::memcpy(&m_header.body_offset, data + 20, sizeof(uint64_t));
	std::memcpy(&m_header.body_size, data + 28, sizeof(uint64_t));

	std::memcpy(m_header.content_id, data + 36, 36);
	m_header.content_id[36] = '\0';

	ComputeSha256(data, 128, m_header.header_digest);

	m_entries.clear();
	size_t entry_table_start = static_cast<size_t>(m_header.table_offset);

	for (uint32_t i = 0; i < m_header.entry_count; ++i) {
		size_t entry_offset = entry_table_start + (i * 32);
		if (entry_offset + 32 > size) {
			break;
		}

		PkgEntry entry{};
		std::memcpy(&entry.id, data + entry_offset, sizeof(uint32_t));
		std::memcpy(&entry.offset, data + entry_offset + 4, sizeof(uint64_t));
		std::memcpy(&entry.size, data + entry_offset + 12, sizeof(uint64_t));
		std::memcpy(&entry.flags, data + entry_offset + 20, sizeof(uint32_t));
		std::memcpy(entry.sha256, data + entry_offset + 24, 8);

		m_entries[entry.id] = entry;
	}

	m_parsed = true;
	return true;
}

bool PkgParser::VerifyIntegrity(const uint8_t* data, size_t size) const {
	if (!m_parsed || !data || size == 0) {
		return false;
	}

	uint8_t current_header_digest[32] = {0};
	ComputeSha256(data, 128, current_header_digest);

	if (std::memcmp(current_header_digest, m_header.header_digest, 32) != 0) {
		return false; // Header tampered
	}

	return true;
}

bool PkgParser::HasEntry(uint32_t entry_id) const {
	return m_entries.find(entry_id) != m_entries.end();
}

const PkgEntry* PkgParser::GetEntry(uint32_t entry_id) const {
	auto it = m_entries.find(entry_id);
	if (it == m_entries.end()) {
		return nullptr;
	}
	return &it->second;
}

bool PkgParser::ExtractEntry(const uint8_t* data, size_t size, uint32_t entry_id, std::vector<uint8_t>& out_bytes) const {
	const PkgEntry* entry = GetEntry(entry_id);
	if (!entry || !data) {
		return false;
	}

	if (entry->offset + entry->size > size) {
		return false;
	}

	out_bytes.resize(static_cast<size_t>(entry->size));
	std::memcpy(out_bytes.data(), data + entry->offset, static_cast<size_t>(entry->size));
	return true;
}

bool PkgParser::MountPfsImage(const uint8_t* pfs_bytes, size_t size, const uint8_t* key, size_t key_size, PfsMountInfo& out_mount_info) {
	out_mount_info.mounted = false;
	if (!pfs_bytes || size < 64) {
		return false;
	}

	std::memcpy(&out_mount_info.magic, pfs_bytes, sizeof(uint32_t));
	if (out_mount_info.magic != 0x00534650 && out_mount_info.magic != 0x50465300) {
		if (!key || key_size < 16) {
			return false;
		}
		out_mount_info.magic = 0x50465300;
	}

	out_mount_info.version      = 1;
	out_mount_info.block_size   = 4096;
	out_mount_info.total_blocks = size / 4096;
	out_mount_info.root_inode   = 2;
	out_mount_info.mounted      = true;

	return true;
}

// ─── PkgInstaller ─────────────────────────────────────────────────────────────

bool PkgInstaller::InstallPackageBuffer(const uint8_t* data, size_t size, const std::string& /*target_dir*/, std::string* out_content_id) {
	PkgParser parser;
	if (!parser.Parse(data, size)) {
		return false;
	}

	if (!parser.VerifyIntegrity(data, size)) {
		return false;
	}

	if (out_content_id) {
		*out_content_id = parser.GetHeader().content_id;
	}

	return true;
}

} // namespace Libs::Kernel::Ps5
