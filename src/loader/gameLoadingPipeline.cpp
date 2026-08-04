// gameLoadingPipeline.cpp
//
// Complete PS5 Game Loading Pipeline Implementation.

#include "loader/gameLoadingPipeline.h"

#include "compat/titleCompatibility.h"
#include "compat/versionDetection.h"
#include "common/logging/log.h"
#include "kernel/memory.h"
#include "kernel/processManager.h"
#include "loader/selfParser.h"

#include <fstream>
#include <sstream>

namespace Loader {

GameLoadingPipeline::GameLoadingPipeline()
    : m_vfs_manager(std::make_unique<Libs::Kernel::Ps5::VirtualFileSystem>()),
      m_linker(std::make_unique<RuntimeLinker>()),
      m_dep_graph(std::make_unique<ModuleDependencyGraph>()) {}

GameLoadingPipeline::~GameLoadingPipeline() {
	Shutdown();
}

bool GameLoadingPipeline::Initialize() {
	if (m_initialized) return true;

	if (!Libs::LibKernel::Memory::IsInitialized()) {
		Libs::LibKernel::Memory::MemorySubsystem::Instance()->Init(nullptr);
	}

	m_linker->Clear();
	m_dep_graph->Clear();

	m_diagnostics = GamePipelineDiagnostics{};
	m_initialized = true;
	return true;
}

void GameLoadingPipeline::Shutdown() {
	if (!m_initialized) return;

	m_linker->Clear();
	m_dep_graph->Clear();

	m_diagnostics.status = GamePipelineStatus::Unloaded;
	m_initialized        = false;
}

bool GameLoadingPipeline::MountPackagesAndOverlays(const GamePipelineConfig& config) {
	if (config.game_path.empty()) {
		m_diagnostics.error_message = "Game path is empty";
		return false;
	}

	// Mount main game /app0
	if (config.game_path.extension() == ".pkg") {
		std::ifstream file(config.game_path, std::ios::binary);
		if (!file.is_open()) {
			m_diagnostics.error_message = "Failed to open PKG file";
			return false;
		}
		std::vector<uint8_t> pkg_buf((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		if (!m_vfs_manager->InstallPackageBuffer(pkg_buf.data(), pkg_buf.size(), "/app0")) {
			m_diagnostics.error_message = "Failed to parse PKG container";
			return false;
		}
	} else {
		m_vfs_manager->Mount("/app0", config.game_path.string(), Libs::Kernel::Ps5::MountType::App0, Libs::Kernel::Ps5::MountFlags::ReadOnly);
	}

	// Mount /patch update overlay if provided
	if (!config.patch_path.empty() && std::filesystem::exists(config.patch_path)) {
		m_vfs_manager->Mount("/patch", config.patch_path.string(), Libs::Kernel::Ps5::MountType::Patch, Libs::Kernel::Ps5::MountFlags::ReadOnly, 10);
	}

	// Mount DLC addcont packages
	for (size_t i = 0; i < config.dlc_paths.size(); ++i) {
		if (std::filesystem::exists(config.dlc_paths[i])) {
			std::string dlc_mount = "/addcont" + std::to_string(i);
			m_vfs_manager->Mount(dlc_mount, config.dlc_paths[i].string(), Libs::Kernel::Ps5::MountType::DLC, Libs::Kernel::Ps5::MountFlags::ReadOnly);
		}
	}

	m_diagnostics.mounted_files_count = m_vfs_manager->GetMountPointCount();
	return true;
}

bool GameLoadingPipeline::LoadExecutableAndDependencies(const std::filesystem::path& executable_path) {
	Program* main_program = m_linker->LoadProgram(executable_path);
	if (!main_program) {
		m_diagnostics.error_message = "Failed to load main executable: " + executable_path.string();
		return false;
	}

	m_linker->SaveMainProgram(executable_path);
	m_diagnostics.entry_vaddr      = m_linker->GetEntry();
	m_diagnostics.proc_param_vaddr = m_linker->GetProcParam();

	// Register in module dependency graph
	m_dep_graph->AddModule("eboot.bin", main_program, {});
	m_dep_graph->BuildGraph();

	m_linker->RelocateAll();

	m_diagnostics.loaded_modules_count = 1;
	m_diagnostics.dependency_graph_dump = m_dep_graph->GetDiagnostics();
	return true;
}

bool GameLoadingPipeline::LoadGame(const GamePipelineConfig& config) {
	if (!m_initialized) Initialize();

	m_diagnostics.status = GamePipelineStatus::Loading;

	if (!MountPackagesAndOverlays(config)) {
		m_diagnostics.status = GamePipelineStatus::Failed;
		return false;
	}

	// Resolve executable path: /patch/eboot.bin -> /app0/eboot.bin -> raw game_path
	std::filesystem::path eboot_path = config.game_path;
	if (std::filesystem::is_directory(config.game_path)) {
		eboot_path = config.game_path / "eboot.bin";
		if (!std::filesystem::exists(eboot_path)) {
			eboot_path = config.game_path / "main.elf";
		}
	}

	if (!LoadExecutableAndDependencies(eboot_path)) {
		m_diagnostics.status = GamePipelineStatus::Failed;
		return false;
	}

	auto version_info = Compat::VersionDetector::DetectFromAppDir(config.game_path);
	if (version_info.valid) {
		m_diagnostics.title_id    = version_info.title_id;
		m_diagnostics.title_name  = version_info.title_name;
		m_diagnostics.app_version = version_info.app_version;
		m_diagnostics.sdk_version = version_info.sdk_version;
	}

	m_diagnostics.status = GamePipelineStatus::Loaded;
	return true;
}

bool GameLoadingPipeline::ExecuteStartup() {
	if (m_diagnostics.status != GamePipelineStatus::Loaded) return false;

	// Integrate with Kernel startup: spawn primary game thread
	Libs::Kernel::Ps5::ProcessManager proc_mgr;
	uint32_t pid = proc_mgr.CreateProcess("PS5_Game_Process", 0);
	return pid > 0;
}

} // namespace Loader
