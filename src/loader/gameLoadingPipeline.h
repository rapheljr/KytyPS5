// gameLoadingPipeline.h
//
// Complete PS5 Game Loading Pipeline (ELF, SELF, PKG, Updates, DLC, Module Graph & Kernel Startup).

#ifndef LOADER_GAME_LOADING_PIPELINE_H
#define LOADER_GAME_LOADING_PIPELINE_H

#include "common/common.h"
#include "kernel/ps5Vfs.h"
#include "loader/moduleDependencyGraph.h"
#include "loader/runtimeLinker.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Loader {

enum class GamePipelineStatus : uint8_t {
	Unloaded = 0,
	Loading,
	Loaded,
	Failed
};

struct GamePipelineConfig {
	std::filesystem::path        game_path;       // PKG or extracted directory
	std::filesystem::path        patch_path;      // Game update /patch overlay
	std::vector<std::filesystem::path> dlc_paths; // DLC /addcont0..N packages
	bool                         verify_integrity = true;
	bool                         auto_relocate    = true;
};

struct GamePipelineDiagnostics {
	GamePipelineStatus  status              = GamePipelineStatus::Unloaded;
	std::string         title_id;
	std::string         title_name;
	std::string         app_version;
	std::string         sdk_version;
	size_t              mounted_files_count = 0;
	size_t              loaded_modules_count = 0;
	uint64_t            entry_vaddr         = 0;
	uint64_t            proc_param_vaddr    = 0;
	std::string         dependency_graph_dump;
	std::string         error_message;
};

class GameLoadingPipeline {
public:
	GameLoadingPipeline();
	~GameLoadingPipeline();

	KYTY_CLASS_NO_COPY(GameLoadingPipeline);

	bool Initialize();
	void Shutdown();

	bool LoadGame(const GamePipelineConfig& config);
	bool ExecuteStartup();

	[[nodiscard]] const GamePipelineDiagnostics& GetDiagnostics() const { return m_diagnostics; }
	[[nodiscard]] RuntimeLinker* GetRuntimeLinker() { return m_linker.get(); }
	[[nodiscard]] Libs::Kernel::Ps5::VirtualFileSystem* GetVfsManager() { return m_vfs_manager.get(); }

private:
	bool MountPackagesAndOverlays(const GamePipelineConfig& config);
	bool LoadExecutableAndDependencies(const std::filesystem::path& executable_path);

	std::unique_ptr<Libs::Kernel::Ps5::VirtualFileSystem> m_vfs_manager;
	std::unique_ptr<RuntimeLinker>                     m_linker;
	std::unique_ptr<ModuleDependencyGraph>            m_dep_graph;
	GamePipelineDiagnostics                            m_diagnostics;
	bool                                               m_initialized = false;
};

} // namespace Loader

#endif // LOADER_GAME_LOADING_PIPELINE_H
