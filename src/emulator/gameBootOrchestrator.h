// gameBootOrchestrator.h
//
// Direct Commercial Game Boot Orchestrator & CLI Runner.
// Coordinates full-system emulator startup, binary format auto-detection (ELF/SELF/PKG),
// subsystem initialization, JIT runtime dispatch, and graphics presentation loop.

#ifndef EMULATOR_GAME_BOOT_ORCHESTRATOR_H
#define EMULATOR_GAME_BOOT_ORCHESTRATOR_H

#include "common/common.h"
#include "kernel/machExceptionHandler.h"
#include "kernel/openOrbisSubsystems.h"
#include "loader/openOrbisElfLoader.h"
#include "loader/recompiler/x86RuntimeBridge.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Emulator {

enum class ExecutableFormat {
	Unknown,
	Elf,
	Self,
	Pkg
};

struct BootResult {
	bool             success         = false;
	ExecutableFormat detected_format = ExecutableFormat::Unknown;
	uint64_t         entry_point_vaddr = 0;
	uint64_t         image_base_vaddr  = 0;
	size_t           symbols_resolved  = 0;
	std::string      error_message;
};

class GameBootOrchestrator {
public:
	GameBootOrchestrator();
	~GameBootOrchestrator();

	KYTY_CLASS_NO_COPY(GameBootOrchestrator);

	/// Detect executable format from magic headers
	static ExecutableFormat DetectFormat(const std::string& file_path);

	/// Boot an executable file (ELF, SELF, or PKG) with complete subsystem orchestration
	BootResult BootExecutable(const std::string& file_path, bool run_main_loop = false);

	/// Run a specified number of frames in the emulation loop
	bool RunFrames(uint32_t frame_count);

	/// Gracefully shut down all orchestrator-managed subsystems
	void Shutdown();

	[[nodiscard]] bool IsBooted() const noexcept { return m_booted; }
	[[nodiscard]] const BootResult& GetBootResult() const noexcept { return m_boot_result; }

private:
	bool                                           m_booted = false;
	BootResult                                     m_boot_result{};
	Loader::Recompiler::JitTelemetryCollector            m_telemetry;
	std::unique_ptr<Kernel::OpenOrbisSubsystemHub>       m_subsystem_hub;
	std::unique_ptr<Kernel::MachExceptionHandler>         m_mach_handler;
	std::unique_ptr<Loader::OpenOrbisElfLoader>          m_loader;
	std::unique_ptr<Libs::Graphics::MetalGraphicBackend> m_metal_backend;
};

} // namespace Emulator

#endif // EMULATOR_GAME_BOOT_ORCHESTRATOR_H
