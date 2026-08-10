// ps5PfsDecryptor.h
//
// PlayStation File System (PFS) Container Parser and Decryptor for PS5 Packages.
// Handles PFS superblock validation, inode tables, and sector decryption.

#ifndef LOADER_PS5_PFS_DECRYPTOR_H
#define LOADER_PS5_PFS_DECRYPTOR_H

#include "common/common.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Loader {

constexpr uint64_t PFS_SUPERBLOCK_MAGIC_A = 0x0000000141534650ULL; // "PFSA"
constexpr uint64_t PFS_SUPERBLOCK_MAGIC_G = 0x0000000147534650ULL; // "PFSG"

#pragma pack(push, 1)
struct PfsSuperBlock {
	uint64_t magic;
	uint64_t version;
	uint32_t flags;
	uint32_t block_size;
	uint64_t total_blocks;
	uint64_t inode_count;
	uint8_t  digest[32];
	uint8_t  reserved[448];
};

struct PfsInode {
	uint32_t mode;
	uint32_t uid;
	uint32_t gid;
	uint32_t link_count;
	uint64_t file_size;
	uint64_t blocks_allocated;
	uint64_t direct_blocks[12];
	uint64_t indirect_block;
};
#pragma pack(pop)

struct PfsEntryInfo {
	std::string name;
	uint64_t    size  = 0;
	bool        is_dir = false;
	uint64_t    block_offset = 0;
};

class Ps5PfsDecryptor {
public:
	Ps5PfsDecryptor();
	~Ps5PfsDecryptor() = default;

	KYTY_CLASS_NO_COPY(Ps5PfsDecryptor);

	/// Load and validate PFS image from raw memory buffer
	bool LoadFromMemory(const uint8_t* data, size_t size);

	/// Set container decryption key (AES-XTS 256-bit or passcode)
	void SetPasscodeKey(const std::string& passcode);

	/// Decrypt a block sector in-place
	bool DecryptBlock(uint8_t* block_data, size_t block_size, uint64_t sector_index);

	[[nodiscard]] bool IsValid() const noexcept { return m_is_valid; }
	[[nodiscard]] const PfsSuperBlock& GetSuperBlock() const noexcept { return m_super_block; }
	[[nodiscard]] const std::vector<PfsEntryInfo>& GetEntries() const noexcept { return m_entries; }

	/// Helper to synthesize a valid mock PFS container image for unit testing
	static std::vector<uint8_t> CreateMockPfsImage(const std::string& test_filename, const std::string& file_content);

private:
	PfsSuperBlock             m_super_block{};
	std::vector<PfsEntryInfo> m_entries;
	std::vector<uint8_t>      m_derived_key;
	bool                      m_is_valid = false;
};

} // namespace Loader

#endif // LOADER_PS5_PFS_DECRYPTOR_H
