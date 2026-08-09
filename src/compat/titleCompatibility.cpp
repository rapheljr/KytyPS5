#include "compat/titleCompatibility.h"

#include "common/stringUtils.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace Compat {

using Json = nlohmann::json;

const char* GameStatusToString(GameStatus status) {
	switch (status) {
		case GameStatus::Nothing: return "Nothing";
		case GameStatus::Boots: return "Boots";
		case GameStatus::Intro: return "Intro";
		case GameStatus::InGame: return "InGame";
		case GameStatus::Playable: return "Playable";
		case GameStatus::Perfect: return "Perfect";
		case GameStatus::Unknown:
		default: return "Unknown";
	}
}

GameStatus GameStatusFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "Nothing")) return GameStatus::Nothing;
	if (Common::EqualNoCase(str, "Boots")) return GameStatus::Boots;
	if (Common::EqualNoCase(str, "Intro")) return GameStatus::Intro;
	if (Common::EqualNoCase(str, "InGame")) return GameStatus::InGame;
	if (Common::EqualNoCase(str, "Playable")) return GameStatus::Playable;
	if (Common::EqualNoCase(str, "Perfect")) return GameStatus::Perfect;
	return GameStatus::Unknown;
}

static std::string IssueCategoryToString(IssueCategory cat) {
	switch (cat) {
		case IssueCategory::Graphics: return "Graphics";
		case IssueCategory::Audio: return "Audio";
		case IssueCategory::Kernel: return "Kernel";
		case IssueCategory::JIT: return "JIT";
		case IssueCategory::Crash: return "Crash";
		case IssueCategory::Performance: return "Performance";
		default: return "Other";
	}
}

static IssueCategory IssueCategoryFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "Graphics")) return IssueCategory::Graphics;
	if (Common::EqualNoCase(str, "Audio")) return IssueCategory::Audio;
	if (Common::EqualNoCase(str, "Kernel")) return IssueCategory::Kernel;
	if (Common::EqualNoCase(str, "JIT")) return IssueCategory::JIT;
	if (Common::EqualNoCase(str, "Crash")) return IssueCategory::Crash;
	if (Common::EqualNoCase(str, "Performance")) return IssueCategory::Performance;
	return IssueCategory::Other;
}

static std::string IssueSeverityToString(IssueSeverity sev) {
	switch (sev) {
		case IssueSeverity::Low: return "Low";
		case IssueSeverity::High: return "High";
		case IssueSeverity::Critical: return "Critical";
		default: return "Medium";
	}
}

static IssueSeverity IssueSeverityFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "Low")) return IssueSeverity::Low;
	if (Common::EqualNoCase(str, "High")) return IssueSeverity::High;
	if (Common::EqualNoCase(str, "Critical")) return IssueSeverity::Critical;
	return IssueSeverity::Medium;
}

static std::string IssueStatusToString(IssueStatus st) {
	switch (st) {
		case IssueStatus::Investigating: return "Investigating";
		case IssueStatus::Mitigated: return "Mitigated";
		case IssueStatus::Resolved: return "Resolved";
		default: return "Open";
	}
}

static IssueStatus IssueStatusFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "Investigating")) return IssueStatus::Investigating;
	if (Common::EqualNoCase(str, "Mitigated")) return IssueStatus::Mitigated;
	if (Common::EqualNoCase(str, "Resolved")) return IssueStatus::Resolved;
	return IssueStatus::Open;
}

static std::string ActionToString(ShaderOverrideAction action) {
	switch (action) {
		case ShaderOverrideAction::DisablePass: return "DisablePass";
		case ShaderOverrideAction::ForceFP16: return "ForceFP16";
		case ShaderOverrideAction::ForceFP32: return "ForceFP32";
		case ShaderOverrideAction::InjectBarrier: return "InjectBarrier";
		case ShaderOverrideAction::ReplaceBytecode: return "ReplaceBytecode";
		case ShaderOverrideAction::ReplaceMSL: return "ReplaceMSL";
		case ShaderOverrideAction::ReplaceSPIRV: return "ReplaceSPIRV";
		default: return "None";
	}
}

static ShaderOverrideAction ActionFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "DisablePass")) return ShaderOverrideAction::DisablePass;
	if (Common::EqualNoCase(str, "ForceFP16")) return ShaderOverrideAction::ForceFP16;
	if (Common::EqualNoCase(str, "ForceFP32")) return ShaderOverrideAction::ForceFP32;
	if (Common::EqualNoCase(str, "InjectBarrier")) return ShaderOverrideAction::InjectBarrier;
	if (Common::EqualNoCase(str, "ReplaceBytecode")) return ShaderOverrideAction::ReplaceBytecode;
	if (Common::EqualNoCase(str, "ReplaceMSL")) return ShaderOverrideAction::ReplaceMSL;
	if (Common::EqualNoCase(str, "ReplaceSPIRV")) return ShaderOverrideAction::ReplaceSPIRV;
	return ShaderOverrideAction::None;
}

static std::string BackendToString(PreferredGpuBackend backend) {
	switch (backend) {
		case PreferredGpuBackend::Vulkan: return "Vulkan";
		case PreferredGpuBackend::Metal: return "Metal";
		default: return "Default";
	}
}

static PreferredGpuBackend BackendFromString(const std::string& str) {
	if (Common::EqualNoCase(str, "Vulkan")) return PreferredGpuBackend::Vulkan;
	if (Common::EqualNoCase(str, "Metal")) return PreferredGpuBackend::Metal;
	return PreferredGpuBackend::Default;
}

bool TitleCompatibilityDatabase::MatchesWildcard(const std::string& pattern, const std::string& text) {
	if (pattern == "*" || pattern.empty() || pattern == text) {
		return true;
	}

	size_t p = 0;
	size_t t = 0;
	size_t star_p = std::string::npos;
	size_t star_t = std::string::npos;

	while (t < text.size()) {
		if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
			p++;
			t++;
		} else if (p < pattern.size() && pattern[p] == '*') {
			star_p = p++;
			star_t = t;
		} else if (star_p != std::string::npos) {
			p = star_p + 1;
			t = ++star_t;
		} else {
			return false;
		}
	}

	while (p < pattern.size() && pattern[p] == '*') {
		p++;
	}

	return p == pattern.size();
}

static Json WriteToJson(const KnownIssue& issue) {
	return Json{
	    {"issue_id", issue.issue_id},
	    {"category", IssueCategoryToString(issue.category)},
	    {"severity", IssueSeverityToString(issue.severity)},
	    {"description", issue.description},
	    {"workaround_applied", issue.workaround_applied},
	    {"status", IssueStatusToString(issue.status)},
	};
}

static KnownIssue ReadKnownIssueFromJson(const Json& j) {
	KnownIssue issue;
	if (j.contains("issue_id") && j["issue_id"].is_string()) issue.issue_id = j["issue_id"];
	if (j.contains("category") && j["category"].is_string()) issue.category = IssueCategoryFromString(j["category"]);
	if (j.contains("severity") && j["severity"].is_string()) issue.severity = IssueSeverityFromString(j["severity"]);
	if (j.contains("description") && j["description"].is_string()) issue.description = j["description"];
	if (j.contains("workaround_applied") && j["workaround_applied"].is_string()) issue.workaround_applied = j["workaround_applied"];
	if (j.contains("status") && j["status"].is_string()) issue.status = IssueStatusFromString(j["status"]);
	return issue;
}

static Json WriteToJson(const GameSpecificPatch& patch) {
	Json writes_arr = Json::array();
	for (const auto& w: patch.writes) {
		writes_arr.push_back({
		    {"address", w.address},
		    {"expected", Common::HexFromBin(Common::ByteBuffer(w.expected.data(), w.expected.size()))},
		    {"replacement", Common::HexFromBin(Common::ByteBuffer(w.replacement.data(), w.replacement.size()))},
		    {"description", w.description},
		});
	}

	Json redirects_arr = Json::array();
	for (const auto& r: patch.symbol_redirects) {
		redirects_arr.push_back({
		    {"nid", r.nid},
		    {"library_name", r.library_name},
		    {"symbol_name", r.symbol_name},
		    {"replacement_addr", r.replacement_addr},
		});
	}

	return Json{
	    {"patch_id", patch.patch_id},
	    {"name", patch.name},
	    {"author", patch.author},
	    {"enabled", patch.enabled},
	    {"writes", writes_arr},
	    {"symbol_redirects", redirects_arr},
	};
}

static GameSpecificPatch ReadGameSpecificPatchFromJson(const Json& j) {
	GameSpecificPatch patch;
	if (j.contains("patch_id") && j["patch_id"].is_string()) patch.patch_id = j["patch_id"];
	if (j.contains("name") && j["name"].is_string()) patch.name = j["name"];
	if (j.contains("author") && j["author"].is_string()) patch.author = j["author"];
	if (j.contains("enabled") && j["enabled"].is_boolean()) patch.enabled = j["enabled"];

	if (j.contains("writes") && j["writes"].is_array()) {
		for (const auto& w_json: j["writes"]) {
			PatchWrite w;
			if (w_json.contains("address") && w_json["address"].is_number_integer()) w.address = w_json["address"];
			if (w_json.contains("expected") && w_json["expected"].is_string()) {
				auto bin = Common::HexToBin(w_json["expected"].get<std::string>());
				const auto* ptr = reinterpret_cast<const uint8_t*>(bin.GetData());
				w.expected.assign(ptr, ptr + bin.Size());
			}
			if (w_json.contains("replacement") && w_json["replacement"].is_string()) {
				auto bin = Common::HexToBin(w_json["replacement"].get<std::string>());
				const auto* ptr = reinterpret_cast<const uint8_t*>(bin.GetData());
				w.replacement.assign(ptr, ptr + bin.Size());
			}
			if (w_json.contains("description") && w_json["description"].is_string()) w.description = w_json["description"];
			patch.writes.push_back(w);
		}
	}

	if (j.contains("symbol_redirects") && j["symbol_redirects"].is_array()) {
		for (const auto& r_json: j["symbol_redirects"]) {
			SymbolRedirect r;
			if (r_json.contains("nid") && r_json["nid"].is_number_integer()) r.nid = r_json["nid"];
			if (r_json.contains("library_name") && r_json["library_name"].is_string()) r.library_name = r_json["library_name"];
			if (r_json.contains("symbol_name") && r_json["symbol_name"].is_string()) r.symbol_name = r_json["symbol_name"];
			if (r_json.contains("replacement_addr") && r_json["replacement_addr"].is_number_integer()) r.replacement_addr = r_json["replacement_addr"];
			patch.symbol_redirects.push_back(r);
		}
	}

	return patch;
}

static Json WriteToJson(const ShaderOverrideRule& rule) {
	return Json{
	    {"rule_id", rule.rule_id},
	    {"shader_hash", rule.shader_hash},
	    {"shader_stage", rule.shader_stage},
	    {"action", ActionToString(rule.action)},
	    {"replacement_source", rule.replacement_source},
	    {"enabled", rule.enabled},
	};
}

static ShaderOverrideRule ReadShaderOverrideFromJson(const Json& j) {
	ShaderOverrideRule rule;
	if (j.contains("rule_id") && j["rule_id"].is_string()) rule.rule_id = j["rule_id"];
	if (j.contains("shader_hash") && j["shader_hash"].is_number_integer()) rule.shader_hash = j["shader_hash"];
	if (j.contains("shader_stage") && j["shader_stage"].is_string()) rule.shader_stage = j["shader_stage"];
	if (j.contains("action") && j["action"].is_string()) rule.action = ActionFromString(j["action"]);
	if (j.contains("replacement_source") && j["replacement_source"].is_string()) rule.replacement_source = j["replacement_source"];
	if (j.contains("enabled") && j["enabled"].is_boolean()) rule.enabled = j["enabled"];
	return rule;
}

static Json WriteToJson(const KernelWorkarounds& k) {
	return Json{
	    {"relaxed_memory_permissions", k.relaxed_memory_permissions},
	    {"dummy_thread_priorities", k.dummy_thread_priorities},
	    {"extended_syscall_stubs", k.extended_syscall_stubs},
	    {"custom_umtx_timeout_ms", k.custom_umtx_timeout_ms},
	    {"virtual_address_padding", k.virtual_address_padding},
	};
}

static KernelWorkarounds ReadKernelWorkaroundsFromJson(const Json& j) {
	KernelWorkarounds k;
	if (j.contains("relaxed_memory_permissions") && j["relaxed_memory_permissions"].is_boolean()) k.relaxed_memory_permissions = j["relaxed_memory_permissions"];
	if (j.contains("dummy_thread_priorities") && j["dummy_thread_priorities"].is_boolean()) k.dummy_thread_priorities = j["dummy_thread_priorities"];
	if (j.contains("extended_syscall_stubs") && j["extended_syscall_stubs"].is_boolean()) k.extended_syscall_stubs = j["extended_syscall_stubs"];
	if (j.contains("custom_umtx_timeout_ms") && j["custom_umtx_timeout_ms"].is_number_integer()) k.custom_umtx_timeout_ms = j["custom_umtx_timeout_ms"];
	if (j.contains("virtual_address_padding") && j["virtual_address_padding"].is_number_integer()) k.virtual_address_padding = j["virtual_address_padding"];
	return k;
}

static Json WriteToJson(const GpuWorkarounds& g) {
	return Json{
	    {"preferred_backend", BackendToString(g.preferred_backend)},
	    {"disable_pipeline_barriers", g.disable_pipeline_barriers},
	    {"force_depth_format_conversion", g.force_depth_format_conversion},
	    {"override_anisotropy", g.override_anisotropy},
	    {"msaa_emulation_mode", g.msaa_emulation_mode},
	    {"command_buffer_flush_threshold", g.command_buffer_flush_threshold},
	};
}

static GpuWorkarounds ReadGpuWorkaroundsFromJson(const Json& j) {
	GpuWorkarounds g;
	if (j.contains("preferred_backend") && j["preferred_backend"].is_string()) g.preferred_backend = BackendFromString(j["preferred_backend"]);
	if (j.contains("disable_pipeline_barriers") && j["disable_pipeline_barriers"].is_boolean()) g.disable_pipeline_barriers = j["disable_pipeline_barriers"];
	if (j.contains("force_depth_format_conversion") && j["force_depth_format_conversion"].is_boolean()) g.force_depth_format_conversion = j["force_depth_format_conversion"];
	if (j.contains("override_anisotropy") && j["override_anisotropy"].is_number_integer()) g.override_anisotropy = j["override_anisotropy"];
	if (j.contains("msaa_emulation_mode") && j["msaa_emulation_mode"].is_number_integer()) g.msaa_emulation_mode = j["msaa_emulation_mode"];
	if (j.contains("command_buffer_flush_threshold") && j["command_buffer_flush_threshold"].is_number_integer()) g.command_buffer_flush_threshold = j["command_buffer_flush_threshold"];
	return g;
}

static Json WriteToJson(const TitleEntry& entry) {
	Json issues_arr = Json::array();
	for (const auto& issue: entry.known_issues) {
		issues_arr.push_back(WriteToJson(issue));
	}

	Json patches_arr = Json::array();
	for (const auto& p: entry.patches) {
		patches_arr.push_back(WriteToJson(p));
	}

	Json shader_arr = Json::array();
	for (const auto& s: entry.shader_overrides) {
		shader_arr.push_back(WriteToJson(s));
	}

	return Json{
	    {"title_id", entry.title_id},
	    {"title_name", entry.title_name},
	    {"app_version", entry.app_version},
	    {"sdk_version", entry.sdk_version},
	    {"status", GameStatusToString(entry.status)},
	    {"known_issues", issues_arr},
	    {"patches", patches_arr},
	    {"shader_overrides", shader_arr},
	    {"kernel_workarounds", WriteToJson(entry.kernel_workarounds)},
	    {"gpu_workarounds", WriteToJson(entry.gpu_workarounds)},
	    {"last_tested_date", entry.last_tested_date},
	    {"notes", entry.notes},
	};
}

static TitleEntry ReadTitleEntryFromJson(const Json& j) {
	TitleEntry entry;
	if (j.contains("title_id") && j["title_id"].is_string()) entry.title_id = j["title_id"];
	if (j.contains("title_name") && j["title_name"].is_string()) entry.title_name = j["title_name"];
	if (j.contains("app_version") && j["app_version"].is_string()) entry.app_version = j["app_version"];
	if (j.contains("sdk_version") && j["sdk_version"].is_string()) entry.sdk_version = j["sdk_version"];
	if (j.contains("status") && j["status"].is_string()) entry.status = GameStatusFromString(j["status"]);
	if (j.contains("last_tested_date") && j["last_tested_date"].is_string()) entry.last_tested_date = j["last_tested_date"];
	if (j.contains("notes") && j["notes"].is_string()) entry.notes = j["notes"];

	if (j.contains("known_issues") && j["known_issues"].is_array()) {
		for (const auto& issue_json: j["known_issues"]) {
			entry.known_issues.push_back(ReadKnownIssueFromJson(issue_json));
		}
	}

	if (j.contains("patches") && j["patches"].is_array()) {
		for (const auto& patch_json: j["patches"]) {
			entry.patches.push_back(ReadGameSpecificPatchFromJson(patch_json));
		}
	}

	if (j.contains("shader_overrides") && j["shader_overrides"].is_array()) {
		for (const auto& shader_json: j["shader_overrides"]) {
			entry.shader_overrides.push_back(ReadShaderOverrideFromJson(shader_json));
		}
	}

	if (j.contains("kernel_workarounds") && j["kernel_workarounds"].is_object()) {
		entry.kernel_workarounds = ReadKernelWorkaroundsFromJson(j["kernel_workarounds"]);
	}

	if (j.contains("gpu_workarounds") && j["gpu_workarounds"].is_object()) {
		entry.gpu_workarounds = ReadGpuWorkaroundsFromJson(j["gpu_workarounds"]);
	}

	return entry;
}

bool TitleCompatibilityDatabase::LoadFromFile(const std::filesystem::path& json_path) {
	std::ifstream file(json_path);
	if (!file.is_open()) {
		return false;
	}

	std::string str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return LoadFromJsonString(str);
}

bool TitleCompatibilityDatabase::SaveToFile(const std::filesystem::path& json_path) const {
	std::ofstream file(json_path);
	if (!file.is_open()) {
		return false;
	}

	file << SaveToJsonString();
	return true;
}

bool TitleCompatibilityDatabase::LoadFromJsonString(const std::string& json_str) {
	auto root = Json::parse(json_str, nullptr, false);
	if (root.is_discarded()) {
		return false;
	}

	m_entries.clear();

	if (root.is_array()) {
		for (const auto& item: root) {
			m_entries.push_back(ReadTitleEntryFromJson(item));
		}
	} else if (root.is_object() && root.contains("entries") && root["entries"].is_array()) {
		for (const auto& item: root["entries"]) {
			m_entries.push_back(ReadTitleEntryFromJson(item));
		}
	}
	return true;
}

std::string TitleCompatibilityDatabase::SaveToJsonString() const {
	Json root = Json::array();
	for (const auto& entry: m_entries) {
		root.push_back(WriteToJson(entry));
	}
	return root.dump(4);
}

void TitleCompatibilityDatabase::RebuildIndexCache() const {
	m_exact_cache.clear();
	for (const auto& entry: m_entries) {
		if (entry.title_id.find('*') == std::string::npos && entry.title_id.find('?') == std::string::npos) {
			m_exact_cache[entry.title_id + ":" + entry.app_version] = &entry;
			m_exact_cache[entry.title_id + ":*"]                   = &entry;
			m_exact_cache[entry.title_id + ":"]                    = &entry;
		}
	}
}

void TitleCompatibilityDatabase::AddOrUpdateEntry(const TitleEntry& entry) {
	for (auto& existing: m_entries) {
		if (MatchesWildcard(existing.title_id, entry.title_id) &&
		    (existing.app_version == entry.app_version || existing.app_version == "*" || entry.app_version.empty())) {
			existing = entry;
			RebuildIndexCache();
			return;
		}
	}
	m_entries.push_back(entry);
	RebuildIndexCache();
}

bool TitleCompatibilityDatabase::RemoveEntry(const std::string& title_id, const std::string& app_version) {
	auto it = std::remove_if(m_entries.begin(), m_entries.end(), [&](const TitleEntry& e) {
		return e.title_id == title_id && (app_version == "*" || e.app_version == app_version);
	});
	if (it != m_entries.end()) {
		m_entries.erase(it, m_entries.end());
		RebuildIndexCache();
		return true;
	}
	return false;
}

const TitleEntry* TitleCompatibilityDatabase::FindEntry(const std::string& title_id, const std::string& app_version) const {
	if (m_exact_cache.empty() && !m_entries.empty()) {
		RebuildIndexCache();
	}

	// Fast O(1) exact hash map lookup
	auto key = title_id + ":" + app_version;
	auto it  = m_exact_cache.find(key);
	if (it != m_exact_cache.end()) {
		return it->second;
	}

	if (!app_version.empty()) {
		auto key_star = title_id + ":*";
		auto it_star  = m_exact_cache.find(key_star);
		if (it_star != m_exact_cache.end()) {
			return it_star->second;
		}
	}

	// Fallback to wildcard search
	const TitleEntry* best_match = nullptr;
	for (const auto& entry: m_entries) {
		if (MatchesWildcard(entry.title_id, title_id)) {
			if (app_version.empty() || entry.app_version == "*" || entry.app_version == app_version) {
				if (entry.title_id == title_id && entry.app_version == app_version) {
					return &entry;
				}
				best_match = &entry;
			}
		}
	}

	return best_match;
}

std::vector<TitleEntry> TitleCompatibilityDatabase::GetAllEntries() const {
	return m_entries;
}

size_t TitleCompatibilityDatabase::GetEntryCount() const {
	return m_entries.size();
}

void TitleCompatibilityDatabase::Clear() {
	m_entries.clear();
	m_exact_cache.clear();
}

void TitleCompatibilityDatabase::PopulateDefaultBuiltinTitles() {
	m_entries.clear();
	m_exact_cache.clear();

	TitleEntry astro;
	astro.title_id        = "PPSA01234";
	astro.title_name      = "Astro's Playroom";
	astro.app_version     = "01.00";
	astro.sdk_version     = "09.00.00";
	astro.status          = GameStatus::InGame;
	astro.last_tested_date = "2026-08-08";
	astro.notes           = "Runs in-game with Metal renderer and native ARM64 JIT.";
	AddOrUpdateEntry(astro);

	TitleEntry demon;
	demon.title_id        = "PPSA01456";
	demon.title_name      = "Demon's Souls";
	demon.app_version     = "01.00";
	demon.sdk_version     = "09.00.00";
	demon.status          = GameStatus::Boots;
	demon.last_tested_date = "2026-08-08";
	demon.notes           = "Boots to introductory cinematics.";
	AddOrUpdateEntry(demon);

	TitleEntry spiderman;
	spiderman.title_id        = "PPSA01789";
	spiderman.title_name      = "Marvel's Spider-Man: Miles Morales";
	spiderman.app_version     = "01.00";
	spiderman.sdk_version     = "09.00.00";
	spiderman.status          = GameStatus::Boots;
	spiderman.last_tested_date = "2026-08-08";
	spiderman.notes           = "Boots with MSL compute shader translation.";
	AddOrUpdateEntry(spiderman);

	TitleEntry ratchet;
	ratchet.title_id        = "PPSA02123";
	ratchet.title_name      = "Ratchet & Clank: Rift Apart";
	ratchet.app_version     = "01.00";
	ratchet.sdk_version     = "09.00.00";
	ratchet.status          = GameStatus::Boots;
	ratchet.last_tested_date = "2026-08-08";
	ratchet.notes           = "Boots with direct block chaining.";
	AddOrUpdateEntry(ratchet);

	TitleEntry returnal;
	returnal.title_id        = "PPSA03456";
	returnal.title_name      = "Returnal";
	returnal.app_version     = "01.00";
	returnal.sdk_version     = "09.00.00";
	returnal.status          = GameStatus::Boots;
	returnal.last_tested_date = "2026-08-08";
	returnal.notes           = "Boots with 7.1.4 3D audio positional routing.";
	AddOrUpdateEntry(returnal);

	TitleEntry homebrew;
	homebrew.title_id        = "HOMEBREW01";
	homebrew.title_name      = "OpenOrbis Sample Homebrew";
	homebrew.app_version     = "01.00";
	homebrew.sdk_version     = "09.00.00";
	homebrew.status          = GameStatus::Playable;
	homebrew.last_tested_date = "2026-08-08";
	homebrew.notes           = "100% playable 60 FPS under native ARM64 JIT.";
	AddOrUpdateEntry(homebrew);
}

} // namespace Compat
