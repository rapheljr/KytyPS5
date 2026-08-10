// ps5PfsDecryptor.cpp
//
// PlayStation File System (PFS) Container Parser and Decryptor Implementation.

#include "loader/ps5PfsDecryptor.h"

#include <cstring>
#include <iostream>

namespace Loader {

Ps5PfsDecryptor::Ps5PfsDecryptor() = default;

void Ps5PfsDecryptor::SetPasscodeKey(const std::string& passcode) {
	m_derived_key.resize(32, 0);
	for (size_t i = 0; i < passcode.size(); ++i) {
		m_derived_key[i % 32] ^= static_cast<uint8_t>(passcode[i]);
	}
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
	if (m_super_block.inode_count > 0 && size >= sizeof(PfsSuperBlock) + sizeof(PfsInode)) {
		PfsInode root_inode{};
		std::memcpy(&root_inode, data + sizeof(PfsSuperBlock), sizeof(PfsInode));

		PfsEntryInfo entry;
		entry.name = "eboot.bin";
		entry.size = root_inode.file_size;
		entry.is_dir = false;
		entry.block_offset = root_inode.direct_blocks[0];
		m_entries.push_back(entry);
	}

	m_is_valid = true;
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

} // namespace Loader
