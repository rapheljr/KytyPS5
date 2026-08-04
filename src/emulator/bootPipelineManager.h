#ifndef KYTY_EMULATOR_BOOT_PIPELINE_MANAGER_H_
#define KYTY_EMULATOR_BOOT_PIPELINE_MANAGER_H_

#include "common/common.h"
#include "compat/compatibilityReporter.h"
#include "compat/titleCompatibility.h"
#include "compat/versionDetection.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Emulator {

enum class BootMilestone : uint8_t {
	Unstarted = 0,
	ElfLoaded,            // Milestone 1: ELF / SELF loaded, mapped, relocated
	KernelStarted,        // Milestone 2: Kernel process created, PCB, memory, VFS
	GpuInitialized,       // Milestone 3: PM4 processor, GPU context, backend ready
	AudioInitialized,     // Milestone 4: CoreAudio HAL, 64-voice mixer, 3D engine
	FirstFrameRendered,   // Milestone 5: Window presentation, swapchain, render targets
	MainMenuAppeared,     // Milestone 6: DualSense input active, UI render loop
	IntroPlaying,         // Milestone 7: Video-out playback, audio/video sync
	GameplayBegins        // Milestone 8: Game loop active, title workarounds
};

const char* BootMilestoneToString(BootMilestone milestone);

struct BootStageLogEntry {
	BootMilestone milestone;
	std::string   description;
	double        timestamp_sec = 0.0;
	bool          success       = true;
};

class BootPipelineManager {
public:
	BootPipelineManager()  = default;
	~BootPipelineManager() = default;

	KYTY_CLASS_NO_COPY(BootPipelineManager);

	static BootPipelineManager* Instance();

	void Reset();
	void StartPipeline(const std::string& title_id, const std::string& app_version);

	bool TransitionToMilestone(BootMilestone target, const std::string& description);
	[[nodiscard]] BootMilestone GetCurrentMilestone() const;
	[[nodiscard]] bool HasReachedMilestone(BootMilestone milestone) const;

	[[nodiscard]] std::vector<BootStageLogEntry> GetBootLogs() const;
	[[nodiscard]] std::string                    GenerateBootReportString() const;

	[[nodiscard]] Compat::CompatibilityReporter* GetReporter() { return &m_reporter; }

private:
	BootMilestone                         m_current_milestone = BootMilestone::Unstarted;
	std::chrono::steady_clock::time_point m_pipeline_start;
	std::vector<BootStageLogEntry>        m_boot_logs;
	Compat::CompatibilityReporter         m_reporter;
	std::string                           m_title_id;
	std::string                           m_app_version;
	mutable std::mutex                    m_mutex;
};

} // namespace Emulator

#endif // KYTY_EMULATOR_BOOT_PIPELINE_MANAGER_H_
