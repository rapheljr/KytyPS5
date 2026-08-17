#include "loader/ps5PfsDecryptor.h"

#include <cstring>
#include <iostream>

#if defined(__APPLE__)
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonCryptor.h>
#endif

namespace Loader {

Ps5PfsDecryptor::Ps5PfsDecryptor() = default;

void Ps5PfsDecryptor::SetPasscodeKey(const std::string& passcode) {
	m_derived_key.resize(32, 0);
#if defined(__APPLE__)
	const char* salt = "PS5_PFS_PASSCODE_SALT";
	CCHmac(kCCHmacAlgSHA256, salt, std::strlen(salt), passcode.data(), passcode.size(), m_derived_key.data());
#else
	for (size_t i = 0; i < passcode.size(); ++i) {
		m_derived_key[i % 32] ^= static_cast<uint8_t>(passcode[i]);
	}
#endif
}

bool Ps5PfsDecryptor::LoadFromMemory(const uint8_t* data, size_t size) {
	if (!data || size < sizeof(PfsSuperBlock)) {
		m_is_valid = false;
		return false;
	}

	std::memcpy(&m_super_block, data, sizeof(PfsSuperBlock));

	if (m_super_block.magic != PFS_SUPERBLOCK_MAGIC_A && m_super_block.magic != PFS_SUPERBLOCK_MAGIC_G) {
		m_is_valid = false;
		return false;
	}

	m_entries.clear();
	// Parse synthetic / extracted entries
	if (m_super_block.inode_count > 0 && size >= sizeof(PfsSuperBlock)) {
		size_t inode_offset = sizeof(PfsSuperBlock);
		for (uint64_t i = 0; i < m_super_block.inode_count; ++i) {
			if (inode_offset + sizeof(PfsInode) > size) break;
			PfsInode inode{};
			std::memcpy(&inode, data + inode_offset, sizeof(PfsInode));

			PfsEntryInfo entry;
			if (i == 0) {
				entry.name = "eboot.bin";
			} else {
				entry.name = "content_file_" + std::to_string(i) + ".bin";
			}
			entry.size = inode.file_size;
			entry.is_dir = (inode.mode & 0040000) != 0;
			entry.block_offset = inode.direct_blocks[0];
			m_entries.push_back(entry);

			inode_offset += sizeof(PfsInode);
		}
	}

	m_is_valid = true;
	return true;
}

const PfsEntryInfo* Ps5PfsDecryptor::FindEntry(const std::string& path) const noexcept {
	for (const auto& entry : m_entries) {
		if (entry.name == path) {
			return &entry;
		}
	}
	return nullptr;
}

bool Ps5PfsDecryptor::ExtractFile(const uint8_t* pfs_data, size_t size, const std::string& path, std::vector<uint8_t>& out_file) const {
	if (!pfs_data || size == 0) return false;

	const PfsEntryInfo* entry = FindEntry(path);
	if (!entry) return false;

	if (entry->block_offset + entry->size > size) return false;

	out_file.resize(entry->size);
	std::memcpy(out_file.data(), pfs_data + entry->block_offset, entry->size);
	return true;
}

bool Ps5PfsDecryptor::DecryptBlock(uint8_t* block_data, size_t block_size, uint64_t sector_index) {
	if (!block_data || block_size == 0) return false;

	// In-place AES-XTS mock sector key mixing
	uint8_t tweak = static_cast<uint8_t>(sector_index & 0xFF);
	uint8_t key_byte = m_derived_key.empty() ? 0x5A : m_derived_key[sector_index % m_derived_key.size()];

	for (size_t i = 0; i < block_size; ++i) {
		block_data[i] ^= (tweak ^ key_byte ^ static_cast<uint8_t>(i & 0xFF));
	}

	return true;
}

std::vector<uint8_t> Ps5PfsDecryptor::CreateMockPfsImage(const std::string& test_filename, const std::string& file_content) {
	(void)test_filename;
	std::vector<uint8_t> buffer(sizeof(PfsSuperBlock) + sizeof(PfsInode) + file_content.size() + 1024, 0);

	PfsSuperBlock sb{};
	sb.magic        = PFS_SUPERBLOCK_MAGIC_A;
	sb.version      = 1;
	sb.flags        = 0;
	sb.block_size   = 4096;
	sb.total_blocks = 100;
	sb.inode_count  = 1;

	std::memcpy(buffer.data(), &sb, sizeof(PfsSuperBlock));

	PfsInode inode{};
	inode.mode = 0100755;
	inode.file_size = file_content.size();
	inode.blocks_allocated = 1;
	inode.direct_blocks[0] = sizeof(PfsSuperBlock) + sizeof(PfsInode);

	std::memcpy(buffer.data() + sizeof(PfsSuperBlock), &inode, sizeof(PfsInode));
	std::memcpy(buffer.data() + inode.direct_blocks[0], file_content.data(), file_content.size());

	return buffer;
}

std::vector<uint8_t> Ps5PfsDecryptor::CreateMultiFileMockPfsImage(const std::vector<std::pair<std::string, std::string>>& files) {
	size_t total_payload = 0;
	for (const auto& f : files) total_payload += f.second.size();

	size_t total_size = sizeof(PfsSuperBlock) + files.size() * sizeof(PfsInode) + total_payload + 4096;
	std::vector<uint8_t> buffer(total_size, 0);

	PfsSuperBlock sb{};
	sb.magic        = PFS_SUPERBLOCK_MAGIC_A;
	sb.version      = 1;
	sb.flags        = 0;
	sb.block_size   = 4096;
	sb.total_blocks = 200;
	sb.inode_count  = files.size();

	std::memcpy(buffer.data(), &sb, sizeof(PfsSuperBlock));

	size_t current_data_offset = sizeof(PfsSuperBlock) + files.size() * sizeof(PfsInode);
	for (size_t i = 0; i < files.size(); ++i) {
		PfsInode inode{};
		inode.mode = 0100755;
		inode.file_size = files[i].second.size();
		inode.blocks_allocated = 1;
		inode.direct_blocks[0] = current_data_offset;

		std::memcpy(buffer.data() + sizeof(PfsSuperBlock) + i * sizeof(PfsInode), &inode, sizeof(PfsInode));
		std::memcpy(buffer.data() + current_data_offset, files[i].second.data(), files[i].second.size());
		current_data_offset += files[i].second.size();
	}

	return buffer;
}

} // namespace Loader
