#include "compat/compatibilityReporter.h"

#include <algorithm>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <numeric>
#include <sstream>

namespace Compat {

using Json = nlohmann::json;

static std::string GetCurrentTimestampString() {
	auto               now = std::chrono::system_clock::now();
	std::time_t        t   = std::chrono::system_clock::to_time_t(now);
	std::ostringstream ss;
	ss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
	return ss.str();
}

void CompatibilityReporter::StartSession(const DetectedVersionInfo& info, const TitleEntry* entry) {
	std::lock_guard<std::mutex> lock(m_mutex);

	m_stats                 = ExecutionSessionStats{};
	m_stats.title_id        = info.title_id.empty() ? "UNKNOWN" : info.title_id;
	m_stats.title_name      = info.title_name.empty() ? "Unknown Title" : info.title_name;
	m_stats.app_version     = info.app_version.empty() ? "01.00" : info.app_version;
	m_stats.sdk_version     = info.sdk_version.empty() ? "09.00.00" : info.sdk_version;
	m_stats.start_timestamp = GetCurrentTimestampString();
	m_start_time            = std::chrono::steady_clock::now();
	m_frame_times.clear();

	if (entry != nullptr) {
		m_stats.session_status = entry->status;
		if (entry->kernel_workarounds.relaxed_memory_permissions) m_stats.active_kernel_workarounds.push_back("RelaxedMemoryPermissions");
		if (entry->kernel_workarounds.dummy_thread_priorities) m_stats.active_kernel_workarounds.push_back("DummyThreadPriorities");
		if (entry->kernel_workarounds.extended_syscall_stubs) m_stats.active_kernel_workarounds.push_back("ExtendedSyscallStubs");
		if (entry->gpu_workarounds.disable_pipeline_barriers) m_stats.active_gpu_workarounds.push_back("DisablePipelineBarriers");
		if (entry->gpu_workarounds.force_depth_format_conversion) m_stats.active_gpu_workarounds.push_back("ForceDepthFormatConversion");
	}
}

void CompatibilityReporter::EndSession(GameStatus final_status) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto end_time             = std::chrono::steady_clock::now();
	m_stats.end_timestamp     = GetCurrentTimestampString();
	m_stats.session_duration_sec =
	    std::chrono::duration<double>(end_time - m_start_time).count();
	m_stats.session_status = final_status;

	if (!m_frame_times.empty()) {
		m_stats.total_frames = m_frame_times.size();
		double sum           = std::accumulate(m_frame_times.begin(), m_frame_times.end(), 0.0);
		double avg_time_ms   = sum / static_cast<double>(m_frame_times.size());
		m_stats.average_fps  = (avg_time_ms > 0.0) ? (1000.0 / avg_time_ms) : 0.0;

		auto minmax              = std::minmax_element(m_frame_times.begin(), m_frame_times.end());
		m_stats.min_frame_time_ms = *minmax.first;
		m_stats.max_frame_time_ms = *minmax.second;
	}
}

void CompatibilityReporter::RecordFrame(double frame_time_ms) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_frame_times.push_back(frame_time_ms);
}

void CompatibilityReporter::RecordPatchApplied(const std::string& patch_name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.applied_patches.push_back(patch_name);
}

void CompatibilityReporter::RecordShaderOverrideApplied(const std::string& rule_name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.applied_shader_overrides.push_back(rule_name);
}

void CompatibilityReporter::RecordKernelWorkaround(const std::string& workaround_name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.active_kernel_workarounds.push_back(workaround_name);
}

void CompatibilityReporter::RecordGpuWorkaround(const std::string& workaround_name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.active_gpu_workarounds.push_back(workaround_name);
}

void CompatibilityReporter::RecordLoadedModule(const std::string& module_name) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.loaded_modules.push_back(module_name);
}

void CompatibilityReporter::RecordFatalError(const std::string& error_msg) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.fatal_errors.push_back(error_msg);
}

void CompatibilityReporter::RecordWarning(const std::string& warning_msg) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.warnings.push_back(warning_msg);
}

ExecutionSessionStats CompatibilityReporter::GetSessionStats() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_stats;
}

std::string CompatibilityReporter::GenerateMarkdownReportString() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::ostringstream           oss;

	oss << "# KytyPS5 Compatibility Session Report: " << m_stats.title_name << " (" << m_stats.title_id << ")\n\n";
	oss << "## Session Summary\n\n";
	oss << "| Parameter | Value |\n";
	oss << "|---|---|\n";
	oss << "| **Title ID** | `" << m_stats.title_id << "` |\n";
	oss << "| **Title Name** | " << m_stats.title_name << " |\n";
	oss << "| **App Version** | " << m_stats.app_version << " |\n";
	oss << "| **SDK Target Version** | " << m_stats.sdk_version << " |\n";
	oss << "| **Compatibility Status** | `" << GameStatusToString(m_stats.session_status) << "` |\n";
	oss << "| **Start Time** | " << m_stats.start_timestamp << " |\n";
	oss << "| **Duration** | " << std::fixed << std::setprecision(2) << m_stats.session_duration_sec << " seconds |\n";
	oss << "| **Total Frames Rendered** | " << m_stats.total_frames << " |\n";
	oss << "| **Average FPS** | " << std::fixed << std::setprecision(1) << m_stats.average_fps << " |\n";
	oss << "| **Frame Time (Min / Max)** | " << std::fixed << std::setprecision(2) << m_stats.min_frame_time_ms << " ms / " << m_stats.max_frame_time_ms << " ms |\n\n";

	oss << "## Applied Patches & Overrides\n\n";
	if (m_stats.applied_patches.empty()) {
		oss << "*No game-specific patches applied.*\n\n";
	} else {
		for (const auto& p: m_stats.applied_patches) {
			oss << "- [Patch] " << p << "\n";
		}
		oss << "\n";
	}

	if (!m_stats.applied_shader_overrides.empty()) {
		oss << "### Shader Overrides\n\n";
		for (const auto& s: m_stats.applied_shader_overrides) {
			oss << "- [Shader Override] " << s << "\n";
		}
		oss << "\n";
	}

	oss << "## Active Workarounds\n\n";
	oss << "### Kernel Workarounds\n\n";
	if (m_stats.active_kernel_workarounds.empty()) {
		oss << "*None.*\n\n";
	} else {
		for (const auto& k: m_stats.active_kernel_workarounds) {
			oss << "- `" << k << "`\n";
		}
		oss << "\n";
	}

	oss << "### GPU Workarounds\n\n";
	if (m_stats.active_gpu_workarounds.empty()) {
		oss << "*None.*\n\n";
	} else {
		for (const auto& g: m_stats.active_gpu_workarounds) {
			oss << "- `" << g << "`\n";
		}
		oss << "\n";
	}

	if (!m_stats.fatal_errors.empty()) {
		oss << "## Fatal Errors & Crashes\n\n";
		for (const auto& err: m_stats.fatal_errors) {
			oss << "> [!CAUTION]\n> " << err << "\n\n";
		}
	}

	return oss.str();
}

std::string CompatibilityReporter::GenerateJsonReportString() const {
	std::lock_guard<std::mutex> lock(m_mutex);

	Json j = {
	    {"title_id", m_stats.title_id},
	    {"title_name", m_stats.title_name},
	    {"app_version", m_stats.app_version},
	    {"sdk_version", m_stats.sdk_version},
	    {"session_status", GameStatusToString(m_stats.session_status)},
	    {"total_frames", m_stats.total_frames},
	    {"average_fps", m_stats.average_fps},
	    {"min_frame_time_ms", m_stats.min_frame_time_ms},
	    {"max_frame_time_ms", m_stats.max_frame_time_ms},
	    {"applied_patches", m_stats.applied_patches},
	    {"applied_shader_overrides", m_stats.applied_shader_overrides},
	    {"active_kernel_workarounds", m_stats.active_kernel_workarounds},
	    {"active_gpu_workarounds", m_stats.active_gpu_workarounds},
	    {"loaded_modules", m_stats.loaded_modules},
	    {"fatal_errors", m_stats.fatal_errors},
	    {"warnings", m_stats.warnings},
	    {"start_timestamp", m_stats.start_timestamp},
	    {"end_timestamp", m_stats.end_timestamp},
	    {"session_duration_sec", m_stats.session_duration_sec},
	};

	return j.dump(4);
}

bool CompatibilityReporter::SaveMarkdownReport(const std::filesystem::path& output_path) const {
	std::ofstream file(output_path);
	if (!file.is_open()) {
		return false;
	}
	file << GenerateMarkdownReportString();
	return true;
}

bool CompatibilityReporter::SaveJsonReport(const std::filesystem::path& output_path) const {
	std::ofstream file(output_path);
	if (!file.is_open()) {
		return false;
	}
	file << GenerateJsonReportString();
	return true;
}

} // namespace Compat
