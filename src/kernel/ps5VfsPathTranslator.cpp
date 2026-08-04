// ps5VfsPathTranslator.cpp
//
// PS5 Path Normalization, Translation & Case-Insensitive Fallback Engine Implementation.

#include "kernel/ps5VfsPathTranslator.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <vector>

namespace Libs::Kernel::Ps5 {

std::string PathTranslator::NormalizePath(const std::string& raw_path) {
	if (raw_path.empty()) return "/";

	std::string s = raw_path;
	std::replace(s.begin(), s.end(), '\\', '/');

	std::stringstream ss(s);
	std::string segment;
	std::vector<std::string> parts;

	while (std::getline(ss, segment, '/')) {
		if (segment.empty() || segment == ".") {
			continue;
		}
		if (segment == "..") {
			if (!parts.empty()) {
				parts.pop_back();
			}
		} else {
			parts.push_back(segment);
		}
	}

	std::string result = "/";
	for (size_t i = 0; i < parts.size(); ++i) {
		result += parts[i];
		if (i + 1 < parts.size()) {
			result += "/";
		}
	}
	return result;
}

std::string PathTranslator::FindCaseInsensitivePath(const std::string& host_path) {
	namespace fs = std::filesystem;
	if (fs::exists(host_path)) {
		return host_path; // Exact match found
	}

	fs::path p(host_path);
	fs::path parent = p.parent_path();
	if (!fs::exists(parent)) {
		return host_path;
	}

	std::string filename = p.filename().string();
	std::string lower_filename = filename;
	std::transform(lower_filename.begin(), lower_filename.end(), lower_filename.begin(), ::tolower);

	std::error_code ec;
	fs::directory_iterator it(parent, ec);
	if (ec) {
		return host_path;
	}

	for (const auto& entry : it) {
		std::string name = entry.path().filename().string();
		std::string lower_name = name;
		std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
		if (lower_name == lower_filename) {
			return entry.path().string();
		}
	}

	return host_path;
}

std::string PathTranslator::TranslateToHostPath(const std::string& guest_path, const VirtualMountManager& mount_mgr) {
	std::string norm = NormalizePath(guest_path);
	std::string raw_host = mount_mgr.ResolveHostPath(norm);
	return FindCaseInsensitivePath(raw_host);
}

} // namespace Libs::Kernel::Ps5
