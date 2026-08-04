#ifndef KYTY_COMPAT_VERSION_DETECTION_H_
#define KYTY_COMPAT_VERSION_DETECTION_H_

#include "common/common.h"

#include <filesystem>
#include <string>

namespace Compat {

struct DetectedVersionInfo {
	std::string title_id;     // e.g. "PPSA01234"
	std::string title_name;   // e.g. "Demon's Souls"
	std::string app_version;  // e.g. "01.05"
	std::string sdk_version;  // e.g. "09.00.00"
	std::string category;     // e.g. "gd"
	std::string content_id;   // e.g. "EP9000-PPSA01234_00-0000000000000000"
	bool        valid = false;
};

class VersionDetector {
public:
	VersionDetector()  = default;
	~VersionDetector() = default;

	KYTY_CLASS_NO_COPY(VersionDetector);

	static DetectedVersionInfo DetectFromAppDir(const std::filesystem::path& app0_dir);
	static DetectedVersionInfo DetectFromParamSfo(const std::filesystem::path& sfo_path);
	static DetectedVersionInfo DetectFromParamJson(const std::filesystem::path& json_path);
};

} // namespace Compat

#endif // KYTY_COMPAT_VERSION_DETECTION_H_
