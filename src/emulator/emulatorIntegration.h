// emulatorIntegration.h
//
// Master Emulator Engine & 7-Stage Boot Pipeline for Phase O Full Integration.

#ifndef EMULATOR_EMULATOR_INTEGRATION_H
#define EMULATOR_EMULATOR_INTEGRATION_H

#include "common/common.h"
#include "graphics/guest_gpu/command_processor/pm4Translator.h"
#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/shader/recompiler/opt/ShaderOptPipeline.h"
#include "kernel/ps5Kernel.h"
#include "kernel/ps5Vfs.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdint>
#include <memory>
#include <string>

namespace Emulator {

enum class EmulatorState : uint8_t {
	Uninitialized = 0,
	Initialized,
	Booting,
	Running,
	Paused,
	Stopped
};

struct EmulatorConfig {
	std::string game_path       = "/app0/eboot.bin";
	std::string save_dir        = "/savedata";
	std::string temp_dir        = "/temp";
	bool        enable_opt      = true;
	uint32_t    target_framerate = 60;
};

struct EngineStats {
	uint64_t frames_rendered     = 0;
	uint64_t total_syscalls      = 0;
	uint64_t pm4_packets_decoded = 0;
	uint64_t shaders_optimized   = 0;
	double   avg_frame_time_ms   = 0.0;
};

class EmulatorEngine {
public:
	explicit EmulatorEngine(const EmulatorConfig& config = EmulatorConfig{});
	~EmulatorEngine();

	KYTY_CLASS_NO_COPY(EmulatorEngine);

	bool Initialize();
	bool BootGame(const std::string& eboot_path);
	bool RunFrame();
	void Shutdown();

	[[nodiscard]] EmulatorState GetState() const noexcept { return m_state; }
	[[nodiscard]] const EngineStats& GetStats() const noexcept { return m_stats; }

	[[nodiscard]] Libs::Kernel::Ps5::SyscallDispatcher& GetSyscallDispatcher() noexcept { return m_syscall_dispatcher; }
	[[nodiscard]] Libs::Kernel::Ps5::VirtualFileSystem& GetVfs() noexcept { return m_vfs; }
	[[nodiscard]] Loader::Recompiler::X86RuntimeBridge& GetRecompilerBridge() noexcept { return m_recompiler_bridge; }
	[[nodiscard]] Libs::Graphics::IGraphicBackend* GetGraphicBackend() noexcept { return m_graphic_backend.get(); }

private:
	EmulatorConfig                             m_config;
	EmulatorState                              m_state = EmulatorState::Uninitialized;
	EngineStats                                m_stats{};

	Libs::Kernel::Ps5::SyscallDispatcher        m_syscall_dispatcher;
	Libs::Kernel::Ps5::ThreadManager           m_thread_manager;
	Libs::Kernel::Ps5::VirtualFileSystem       m_vfs;
	Loader::Recompiler::X86RuntimeBridge       m_recompiler_bridge;
	Libs::Graphics::ShaderRecompiler::Opt::ShaderOptPassManager m_shader_opt_manager;
	std::unique_ptr<Libs::Graphics::IGraphicBackend> m_graphic_backend;
};

} // namespace Emulator

#endif // EMULATOR_EMULATOR_INTEGRATION_H
