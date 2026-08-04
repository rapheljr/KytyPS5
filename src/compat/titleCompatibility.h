#ifndef KYTY_COMPAT_TITLE_COMPATIBILITY_H_
#define KYTY_COMPAT_TITLE_COMPATIBILITY_H_

#include "common/common.h"

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Compat {

enum class GameStatus {
	Unknown = 0,
	Nothing,
	Boots,
	Intro,
	InGame,
	Playable,
	Perfect
};

const char* GameStatusToString(GameStatus status);
GameStatus  GameStatusFromString(const std::string& str);

enum class IssueCategory {
	Graphics = 0,
	Audio,
	Kernel,
	JIT,
	Crash,
	Performance,
	Other
};

enum class IssueSeverity {
	Low = 0,
	Medium,
	High,
	Critical
};

enum class IssueStatus {
	Open = 0,
	Investigating,
	Mitigated,
	Resolved
};

struct KnownIssue {
	std::string   issue_id;
	IssueCategory category = IssueCategory::Other;
	IssueSeverity severity = IssueSeverity::Medium;
	std::string   description;
	std::string   workaround_applied;
	IssueStatus   status = IssueStatus::Open;
};

struct PatchWrite {
	uint64_t             address = 0;
	std::vector<uint8_t> expected;
	std::vector<uint8_t> replacement;
	std::string          description;
};

struct SymbolRedirect {
	uint32_t    nid = 0;
	std::string library_name;
	std::string symbol_name;
	uint64_t    replacement_addr = 0;
};

struct GameSpecificPatch {
	std::string                 patch_id;
	std::string                 name;
	std::string                 author;
	std::vector<PatchWrite>     writes;
	std::vector<SymbolRedirect> symbol_redirects;
	bool                        enabled = true;
};

enum class ShaderOverrideAction {
	None = 0,
	DisablePass,
	ForceFP16,
	ForceFP32,
	InjectBarrier,
	ReplaceBytecode,
	ReplaceMSL,
	ReplaceSPIRV
};

struct ShaderOverrideRule {
	std::string          rule_id;
	uint64_t             shader_hash = 0;
	std::string          shader_stage; // "vertex", "fragment", "compute", "all"
	ShaderOverrideAction action = ShaderOverrideAction::None;
	std::vector<uint8_t> replacement_bytecode;
	std::string          replacement_source;
	bool                 enabled = true;
};

struct KernelWorkarounds {
	bool     relaxed_memory_permissions = false;
	bool     dummy_thread_priorities   = false;
	bool     extended_syscall_stubs    = false;
	uint32_t custom_umtx_timeout_ms     = 0;
	uint64_t virtual_address_padding    = 0;
};

enum class PreferredGpuBackend {
	Default = 0,
	Vulkan,
	Metal
};

struct GpuWorkarounds {
	PreferredGpuBackend preferred_backend             = PreferredGpuBackend::Default;
	bool                disable_pipeline_barriers     = false;
	bool                force_depth_format_conversion = false;
	uint32_t            override_anisotropy            = 0; // 0 = default, 1..16
	uint32_t            msaa_emulation_mode            = 0; // 0 = off, 1 = forced 1x, 2 = resolve
	uint32_t            command_buffer_flush_threshold = 0; // 0 = default, >0 = flush after N draws
};

struct TitleEntry {
	std::string                     title_id;
	std::string                     title_name;
	std::string                     app_version; // e.g. "01.00", "*"
	std::string                     sdk_version; // e.g. "09.00.00"
	GameStatus                      status = GameStatus::Unknown;
	std::vector<KnownIssue>         known_issues;
	std::vector<GameSpecificPatch>  patches;
	std::vector<ShaderOverrideRule> shader_overrides;
	KernelWorkarounds               kernel_workarounds;
	GpuWorkarounds                  gpu_workarounds;
	std::string                     last_tested_date;
	std::string                     notes;
};

class TitleCompatibilityDatabase {
public:
	TitleCompatibilityDatabase()  = default;
	~TitleCompatibilityDatabase() = default;

	KYTY_CLASS_NO_COPY(TitleCompatibilityDatabase);

	bool LoadFromFile(const std::filesystem::path& json_path);
	bool SaveToFile(const std::filesystem::path& json_path) const;

	bool LoadFromJsonString(const std::string& json_str);
	[[nodiscard]] std::string SaveToJsonString() const;

	void AddOrUpdateEntry(const TitleEntry& entry);
	bool RemoveEntry(const std::string& title_id, const std::string& app_version = "*");

	[[nodiscard]] const TitleEntry* FindEntry(const std::string& title_id,
	                                          const std::string& app_version = "") const;

	[[nodiscard]] std::vector<TitleEntry> GetAllEntries() const;
	[[nodiscard]] size_t                  GetEntryCount() const;
	void                                  Clear();

	static bool MatchesWildcard(const std::string& pattern, const std::string& text);

private:
	void RebuildIndexCache() const;

	std::vector<TitleEntry>                                m_entries;
	mutable std::unordered_map<std::string, const TitleEntry*> m_exact_cache;
};

} // namespace Compat

#endif // KYTY_COMPAT_TITLE_COMPATIBILITY_H_
