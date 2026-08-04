#include "emulator/bootPipelineManager.h"

#include "common/logging/log.h"
#include "common/singleton.h"

#include <iomanip>
#include <sstream>

namespace Emulator {

const char* BootMilestoneToString(BootMilestone milestone) {
	switch (milestone) {
		case BootMilestone::ElfLoaded: return "Milestone 1: ELF Loaded";
		case BootMilestone::KernelStarted: return "Milestone 2: Kernel Started";
		case BootMilestone::GpuInitialized: return "Milestone 3: GPU Initialized";
		case BootMilestone::AudioInitialized: return "Milestone 4: Audio Initialized";
		case BootMilestone::FirstFrameRendered: return "Milestone 5: First Frame Rendered";
		case BootMilestone::MainMenuAppeared: return "Milestone 6: Main Menu Appeared";
		case BootMilestone::IntroPlaying: return "Milestone 7: Intro Playing";
		case BootMilestone::GameplayBegins: return "Milestone 8: Gameplay Begins";
		case BootMilestone::Unstarted:
		default: return "Unstarted";
	}
}

BootPipelineManager* BootPipelineManager::Instance() {
	return Common::Singleton<BootPipelineManager>::Instance();
}

void BootPipelineManager::Reset() {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_current_milestone = BootMilestone::Unstarted;
	m_boot_logs.clear();
	m_title_id.clear();
	m_app_version.clear();
}

void BootPipelineManager::StartPipeline(const std::string& title_id, const std::string& app_version) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_current_milestone = BootMilestone::Unstarted;
	m_pipeline_start    = std::chrono::steady_clock::now();
	m_title_id          = title_id;
	m_app_version       = app_version;
	m_boot_logs.clear();

	Compat::DetectedVersionInfo info;
	info.title_id    = title_id;
	info.app_version = app_version;
	info.valid       = true;
	m_reporter.StartSession(info);

	LOGF_COLOR(Log::Color::Cyan, "[BOOT_PIPELINE] Starting Title Boot Pipeline for %s (v%s)...\n",
	           title_id.c_str(), app_version.c_str());
}

bool BootPipelineManager::TransitionToMilestone(BootMilestone target, const std::string& description) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto now            = std::chrono::steady_clock::now();
	double elapsed_sec  = std::chrono::duration<double>(now - m_pipeline_start).count();
	m_current_milestone = target;

	BootStageLogEntry entry;
	entry.milestone     = target;
	entry.description   = description;
	entry.timestamp_sec = elapsed_sec;
	entry.success       = true;
	m_boot_logs.push_back(entry);

	LOGF_COLOR(Log::Color::BrightGreen, "[BOOT_STAGE] %s reached at +%.3f s: %s\n",
	           BootMilestoneToString(target), elapsed_sec, description.c_str());

	return true;
}

BootMilestone BootPipelineManager::GetCurrentMilestone() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_current_milestone;
}

bool BootPipelineManager::HasReachedMilestone(BootMilestone milestone) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return static_cast<uint8_t>(m_current_milestone) >= static_cast<uint8_t>(milestone);
}

std::vector<BootStageLogEntry> BootPipelineManager::GetBootLogs() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_boot_logs;
}

std::string BootPipelineManager::GenerateBootReportString() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	std::ostringstream           ss;

	ss << "====================================================\n";
	ss << " KytyPS5 Commercial Title Boot Pipeline Report      \n";
	ss << " Title ID: " << (m_title_id.empty() ? "UNKNOWN" : m_title_id)
	   << " (v" << (m_app_version.empty() ? "01.00" : m_app_version) << ")\n";
	ss << " Current Milestone: " << BootMilestoneToString(m_current_milestone) << "\n";
	ss << "====================================================\n\n";

	for (const auto& log: m_boot_logs) {
		ss << std::fixed << std::setprecision(3) << "  [+" << log.timestamp_sec << " s] "
		   << BootMilestoneToString(log.milestone) << ": " << log.description << "\n";
	}

	ss << "\n====================================================\n";
	return ss.str();
}

} // namespace Emulator
