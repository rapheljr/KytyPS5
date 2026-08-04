#ifndef KYTY_COMPAT_COMPAT_DEV_TOOLS_H_
#define KYTY_COMPAT_COMPAT_DEV_TOOLS_H_

#include "common/common.h"
#include "compat/titleCompatibility.h"

#include <filesystem>
#include <string>
#include <vector>

namespace Compat {

struct ValidationResult {
	bool                     valid = true;
	std::vector<std::string> errors;
	std::vector<std::string> warnings;
};

class TitleCompatDevTools {
public:
	TitleCompatDevTools()  = default;
	~TitleCompatDevTools() = default;

	KYTY_CLASS_NO_COPY(TitleCompatDevTools);

	static ValidationResult ValidateDatabaseFile(const std::filesystem::path& json_path);
	static ValidationResult ValidateTitleEntry(const TitleEntry& entry);

	static bool AddKnownIssue(const std::filesystem::path& db_path,
	                          const std::string& title_id,
	                          const KnownIssue& issue);

	static bool AddGamePatch(const std::filesystem::path& db_path,
	                         const std::string& title_id,
	                         const GameSpecificPatch& patch);

	static bool AddShaderOverride(const std::filesystem::path& db_path,
	                              const std::string& title_id,
	                              const ShaderOverrideRule& rule);

	static bool ExportMarkdownMatrix(const std::filesystem::path& db_path,
	                                 const std::filesystem::path& output_md_path);

	static int RunCliTool(int argc, const char* argv[]);
};

} // namespace Compat

#endif // KYTY_COMPAT_COMPAT_DEV_TOOLS_H_
