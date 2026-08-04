#include "compat/compatDevTools.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace Compat {

ValidationResult TitleCompatDevTools::ValidateTitleEntry(const TitleEntry& entry) {
	ValidationResult res;

	if (entry.title_id.empty()) {
		res.valid = false;
		res.errors.push_back("Title ID cannot be empty.");
	} else if (entry.title_id.length() != 9 && entry.title_id.find('*') == std::string::npos) {
		res.warnings.push_back("Title ID '" + entry.title_id + "' does not match standard 9-character format (e.g. PPSA01234).");
	}

	if (entry.status == GameStatus::Unknown) {
		res.warnings.push_back("Title '" + entry.title_id + "' has Unknown compatibility status.");
	}

	for (const auto& patch: entry.patches) {
		for (const auto& w: patch.writes) {
			if (!w.expected.empty() && !w.replacement.empty() && w.expected.size() != w.replacement.size()) {
				res.valid = false;
				res.errors.push_back("Patch '" + patch.patch_id + "' has mismatched write lengths (expected " +
				                     std::to_string(w.expected.size()) + " B vs replacement " +
				                     std::to_string(w.replacement.size()) + " B).");
			}
		}
	}

	for (const auto& s: entry.shader_overrides) {
		if (s.shader_hash == 0 && s.rule_id.empty()) {
			res.warnings.push_back("Shader override rule has hash 0 and empty rule_id.");
		}
	}

	return res;
}

ValidationResult TitleCompatDevTools::ValidateDatabaseFile(const std::filesystem::path& json_path) {
	ValidationResult res;

	TitleCompatibilityDatabase db;
	if (!db.LoadFromFile(json_path)) {
		res.valid = false;
		res.errors.push_back("Failed to parse JSON file at: " + json_path.string());
		return res;
	}

	for (const auto& entry: db.GetAllEntries()) {
		auto entry_res = ValidateTitleEntry(entry);
		if (!entry_res.valid) {
			res.valid = false;
		}
		res.errors.insert(res.errors.end(), entry_res.errors.begin(), entry_res.errors.end());
		res.warnings.insert(res.warnings.end(), entry_res.warnings.begin(), entry_res.warnings.end());
	}

	return res;
}

bool TitleCompatDevTools::AddKnownIssue(const std::filesystem::path& db_path,
                                        const std::string& title_id,
                                        const KnownIssue& issue) {
	TitleCompatibilityDatabase db;
	db.LoadFromFile(db_path);

	const auto* existing = db.FindEntry(title_id);
	TitleEntry  entry;
	if (existing != nullptr) {
		entry = *existing;
	} else {
		entry.title_id   = title_id;
		entry.app_version = "*";
		entry.status     = GameStatus::InGame;
	}

	entry.known_issues.push_back(issue);
	db.AddOrUpdateEntry(entry);

	return db.SaveToFile(db_path);
}

bool TitleCompatDevTools::AddGamePatch(const std::filesystem::path& db_path,
                                       const std::string& title_id,
                                       const GameSpecificPatch& patch) {
	TitleCompatibilityDatabase db;
	db.LoadFromFile(db_path);

	const auto* existing = db.FindEntry(title_id);
	TitleEntry  entry;
	if (existing != nullptr) {
		entry = *existing;
	} else {
		entry.title_id   = title_id;
		entry.app_version = "*";
	}

	entry.patches.push_back(patch);
	db.AddOrUpdateEntry(entry);

	return db.SaveToFile(db_path);
}

bool TitleCompatDevTools::AddShaderOverride(const std::filesystem::path& db_path,
                                            const std::string& title_id,
                                            const ShaderOverrideRule& rule) {
	TitleCompatibilityDatabase db;
	db.LoadFromFile(db_path);

	const auto* existing = db.FindEntry(title_id);
	TitleEntry  entry;
	if (existing != nullptr) {
		entry = *existing;
	} else {
		entry.title_id   = title_id;
		entry.app_version = "*";
	}

	entry.shader_overrides.push_back(rule);
	db.AddOrUpdateEntry(entry);

	return db.SaveToFile(db_path);
}

bool TitleCompatDevTools::ExportMarkdownMatrix(const std::filesystem::path& db_path,
                                               const std::filesystem::path& output_md_path) {
	TitleCompatibilityDatabase db;
	if (!db.LoadFromFile(db_path)) {
		return false;
	}

	std::ofstream file(output_md_path);
	if (!file.is_open()) {
		return false;
	}

	file << "# KytyPS5 Title Compatibility Matrix\n\n";
	file << "Total Titles In Database: **" << db.GetEntryCount() << "**\n\n";
	file << "| Title ID | Title Name | App Version | Status | Known Issues | Patches | Last Tested |\n";
	file << "|---|---|---|---|---|---|---|\n";

	for (const auto& entry: db.GetAllEntries()) {
		file << "| `" << entry.title_id << "` | "
		     << (entry.title_name.empty() ? "-" : entry.title_name) << " | "
		     << entry.app_version << " | `"
		     << GameStatusToString(entry.status) << "` | "
		     << entry.known_issues.size() << " issue(s) | "
		     << entry.patches.size() << " patch(es) | "
		     << (entry.last_tested_date.empty() ? "-" : entry.last_tested_date) << " |\n";
	}

	file << "\n*Generated automatically by KytyPS5 Compatibility Developer Tools.*\n";
	return true;
}

int TitleCompatDevTools::RunCliTool(int argc, const char* argv[]) {
	if (argc < 2) {
		std::cout << "Usage: kyty-compat-tool <validate|export|add-issue> [args...]\n";
		return 1;
	}

	std::string cmd = argv[1];
	if (cmd == "validate" && argc >= 3) {
		auto res = ValidateDatabaseFile(argv[2]);
		if (res.valid) {
			std::cout << "[OK] Compatibility database is VALID (" << res.warnings.size() << " warnings).\n";
			for (const auto& w: res.warnings) std::cout << "  [WARN] " << w << "\n";
			return 0;
		}
		std::cout << "[FAIL] Compatibility database contains ERRORS:\n";
		for (const auto& err: res.errors) std::cout << "  [ERR] " << err << "\n";
		return 2;
	}

	if (cmd == "export" && argc >= 4) {
		if (ExportMarkdownMatrix(argv[2], argv[3])) {
			std::cout << "[OK] Compatibility matrix exported to: " << argv[3] << "\n";
			return 0;
		}
		std::cout << "[FAIL] Export failed.\n";
		return 3;
	}

	std::cout << "Unknown command or invalid arguments.\n";
	return 1;
}

} // namespace Compat
