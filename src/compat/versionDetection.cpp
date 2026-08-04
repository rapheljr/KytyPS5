#include "compat/versionDetection.h"

#include "common/stringUtils.h"
#include "loader/systemContent.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace Compat {

using Json = nlohmann::json;

DetectedVersionInfo VersionDetector::DetectFromParamSfo(const std::filesystem::path& sfo_path) {
	DetectedVersionInfo info;
	if (!std::filesystem::exists(sfo_path)) {
		return info;
	}

	std::string val;
	if (Loader::SystemContentParamSfoGetString("TITLE_ID", &val) && !val.empty()) {
		info.title_id = val;
	}
	if (Loader::SystemContentParamSfoGetString("TITLE", &val) && !val.empty()) {
		info.title_name = val;
	}
	if (Loader::SystemContentParamSfoGetString("APP_VER", &val) && !val.empty()) {
		info.app_version = val;
	}
	if (Loader::SystemContentParamSfoGetString("SYSTEM_VER", &val) && !val.empty()) {
		info.sdk_version = val;
	}
	if (Loader::SystemContentParamSfoGetString("CATEGORY", &val) && !val.empty()) {
		info.category = val;
	}
	if (Loader::SystemContentParamSfoGetString("CONTENT_ID", &val) && !val.empty()) {
		info.content_id = val;
	}

	info.valid = !info.title_id.empty() || !info.title_name.empty();
	return info;
}

DetectedVersionInfo VersionDetector::DetectFromParamJson(const std::filesystem::path& json_path) {
	DetectedVersionInfo info;
	std::ifstream file(json_path);
	if (!file.is_open()) {
		return info;
	}

	Json root = Json::parse(file, nullptr, false);
	if (root.is_discarded()) {
		return info;
	}

	if (root.contains("titleId") && root["titleId"].is_string()) {
		info.title_id = root["titleId"].get<std::string>();
	}
	if (root.contains("titleName") && root["titleName"].is_string()) {
		info.title_name = root["titleName"].get<std::string>();
	}
	if (root.contains("appVersion") && root["appVersion"].is_string()) {
		info.app_version = root["appVersion"].get<std::string>();
	}
	if (root.contains("sdkVersion") && root["sdkVersion"].is_string()) {
		info.sdk_version = root["sdkVersion"].get<std::string>();
	}
	if (root.contains("contentId") && root["contentId"].is_string()) {
		info.content_id = root["contentId"].get<std::string>();
	}
	info.valid = !info.title_id.empty();

	return info;
}

DetectedVersionInfo VersionDetector::DetectFromAppDir(const std::filesystem::path& app0_dir) {
	DetectedVersionInfo info;

	auto sfo_path = app0_dir / "sce_sys" / "param.sfo";
	if (std::filesystem::exists(sfo_path)) {
		info = DetectFromParamSfo(sfo_path);
		if (info.valid) {
			return info;
		}
	}

	auto json_path = app0_dir / "sce_sys" / "param.json";
	if (std::filesystem::exists(json_path)) {
		info = DetectFromParamJson(json_path);
		if (info.valid) {
			return info;
		}
	}

	// Fallback to directory name parsing
	std::string dir_name = app0_dir.filename().string();
	if (dir_name.length() >= 9 && dir_name.substr(4, 5).find_first_not_of("0123456789") == std::string::npos) {
		info.title_id    = dir_name.substr(0, 9);
		info.app_version = "01.00";
		info.valid       = true;
	}

	return info;
}

} // namespace Compat
