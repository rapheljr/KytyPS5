#ifndef KYTY_COMPAT_COMPATIBILITY_REPORTER_H_
#define KYTY_COMPAT_COMPATIBILITY_REPORTER_H_

#include "common/common.h"
#include "compat/titleCompatibility.h"
#include "compat/versionDetection.h"

#include <chrono>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace Compat {

struct ExecutionSessionStats {
	std::string title_id;
	std::string title_name;
	std::string app_version;
	std::string sdk_version;
	GameStatus  session_status = GameStatus::Unknown;

	uint64_t total_frames        = 0;
	double   average_fps         = 0.0;
	double   min_frame_time_ms   = 0.0;
	double   max_frame_time_ms   = 0.0;
	uint32_t active_thread_count = 0;

	std::vector<std::string> applied_patches;
	std::vector<std::string> applied_shader_overrides;
	std::vector<std::string> active_kernel_workarounds;
	std::vector<std::string> active_gpu_workarounds;
	std::vector<std::string> loaded_modules;
	std::vector<std::string> fatal_errors;
	std::vector<std::string> warnings;

	std::string start_timestamp;
	std::string end_timestamp;
	double      session_duration_sec = 0.0;
};

class CompatibilityReporter {
public:
	CompatibilityReporter()  = default;
	~CompatibilityReporter() = default;

	KYTY_CLASS_NO_COPY(CompatibilityReporter);

	void StartSession(const DetectedVersionInfo& info, const TitleEntry* entry = nullptr);
	void EndSession(GameStatus final_status);

	void RecordFrame(double frame_time_ms);
	void RecordPatchApplied(const std::string& patch_name);
	void RecordShaderOverrideApplied(const std::string& rule_name);
	void RecordKernelWorkaround(const std::string& workaround_name);
	void RecordGpuWorkaround(const std::string& workaround_name);
	void RecordLoadedModule(const std::string& module_name);
	void RecordFatalError(const std::string& error_msg);
	void RecordWarning(const std::string& warning_msg);

	[[nodiscard]] ExecutionSessionStats GetSessionStats() const;

	bool SaveMarkdownReport(const std::filesystem::path& output_path) const;
	bool SaveJsonReport(const std::filesystem::path& output_path) const;

	[[nodiscard]] std::string GenerateMarkdownReportString() const;
	[[nodiscard]] std::string GenerateJsonReportString() const;

private:
	ExecutionSessionStats                        m_stats;
	std::chrono::steady_clock::time_point        m_start_time;
	std::vector<double>                          m_frame_times;
	mutable std::mutex                           m_mutex;
};

} // namespace Compat

#endif // KYTY_COMPAT_COMPATIBILITY_REPORTER_H_
