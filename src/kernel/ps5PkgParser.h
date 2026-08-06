// ps5PkgParser.h
//
// PS5 Package (.pkg) Header Parser, Entry Extractor & SHA-256 Integrity Verifier.

#ifndef KERNEL_PS5_PKG_PARSER_H
#define KERNEL_PS5_PKG_PARSER_H

#include "common/common.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

constexpr uint32_t kPkgMagicCNT = 0x7F434E54; // "\x7FCNT"
constexpr uint32_t kPkgMagicPKG = 0x7F504B47; // "\x7FPKG"

enum class PkgType : uint32_t {
	Unknown = 0,
	App,
	Patch,
	DLC,
	System
};

enum class PkgEntryId : uint32_t {
	ParamSfo   = 0x0001,
	Icon0Png   = 0x0002,
	Pic0Png    = 0x0003,
	EbootBin   = 0x1000,
	PfsImage   = 0x1001,
	LicenseDat = 0x1008
};

struct PkgEntry {
	uint32_t id            = 0;
	uint64_t offset        = 0;
	uint64_t size          = 0;
	uint32_t flags         = 0;
	uint8_t  sha256[32]    = {0};
	bool     is_encrypted  = false;
};

struct PkgHeader {
	uint32_t magic          = 0;
	PkgType  type           = PkgType::Unknown;
	uint32_t entry_count    = 0;
	uint64_t table_offset   = 0;
	uint64_t body_offset    = 0;
	uint64_t body_size      = 0;
	char     content_id[37] = {0}; // e.g. "HP0700-PPSA01234_00-GAME000000000000"
	uint8_t  header_digest[32] = {0};
};

struct PfsMountInfo {
	uint32_t magic        = 0;
	uint32_t version      = 0;
	uint32_t block_size   = 0;
	uint64_t total_blocks = 0;
	uint64_t root_inode   = 0;
	bool     mounted      = false;
};

class PkgParser {
public:
	PkgParser() = default;
	~PkgParser() = default;

	KYTY_CLASS_NO_COPY(PkgParser);

	bool Parse(const uint8_t* data, size_t size);
	bool VerifyIntegrity(const uint8_t* data, size_t size) const;
	static bool MountPfsImage(const uint8_t* pfs_bytes, size_t size, const uint8_t* key, size_t key_size, PfsMountInfo& out_mount_info);

	[[nodiscard]] const PkgHeader& GetHeader() const noexcept { return m_header; }
	[[nodiscard]] const std::unordered_map<uint32_t, PkgEntry>& GetEntries() const noexcept { return m_entries; }
	[[nodiscard]] bool HasEntry(uint32_t entry_id) const;
	[[nodiscard]] const PkgEntry* GetEntry(uint32_t entry_id) const;

	bool ExtractEntry(const uint8_t* data, size_t size, uint32_t entry_id, std::vector<uint8_t>& out_bytes) const;

private:
	static void ComputeSha256(const uint8_t* data, size_t size, uint8_t out_hash[32]);

	PkgHeader                                 m_header{};
	std::unordered_map<uint32_t, PkgEntry>    m_entries;
	bool                                      m_parsed = false;
};

class PkgInstaller {
public:
	PkgInstaller() = default;
	~PkgInstaller() = default;

	KYTY_CLASS_NO_COPY(PkgInstaller);

	static bool InstallPackageBuffer(const uint8_t* data, size_t size, const std::string& target_dir, std::string* out_content_id = nullptr);
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_PKG_PARSER_H
