// gameBootOrchestrator.cpp
//
// Direct Commercial Game Boot Orchestrator & CLI Runner Implementation.

#include "emulator/gameBootOrchestrator.h"
#include "kernel/machExceptionHandler.h"
#include "kernel/ps5PkgParser.h"
#include "loader/selfParser.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include <vector>

namespace Emulator {

ExecutableFormat GameBootOrchestrator::DetectFormat(const std::string& file_path) {
	std::ifstream in(file_path, std::ios::binary);
	if (!in) return ExecutableFormat::Unknown;

	uint8_t magic[16] = {0};
	in.read(reinterpret_cast<char*>(magic), sizeof(magic));
	if (in.gcount() < 4) return ExecutableFormat::Unknown;

	// ELF: 0x7F 'E' 'L' 'F'
	if (magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F') {
		return ExecutableFormat::Elf;
	}

	// SELF: 0x4F 'I' 'S' 'P' or 'S' 'C' 'E' 0
	if ((magic[0] == 0x4F && magic[1] == 0x15 && magic[2] == 0x3D && magic[3] == 0x1D) ||
	    (magic[0] == 'S' && magic[1] == 'C' && magic[2] == 'E' && magic[3] == 0)) {
		return ExecutableFormat::Self;
	}

	// PKG: 0x7F 'C' 'N' 'T'
	if (magic[0] == 0x7F && magic[1] == 'C' && magic[2] == 'N' && magic[3] == 'T') {
		return ExecutableFormat::Pkg;
	}

	return ExecutableFormat::Unknown;
}

GameBootOrchestrator::GameBootOrchestrator() = default;

GameBootOrchestrator::~GameBootOrchestrator() {
	Shutdown();
}

BootResult GameBootOrchestrator::BootExecutable(const std::string& file_path, bool run_main_loop) {
	Shutdown();

	m_boot_result = BootResult{};
	m_boot_result.detected_format = DetectFormat(file_path);

	if (m_boot_result.detected_format == ExecutableFormat::Unknown) {
		m_boot_result.error_message = "Unrecognized or inaccessible binary format";
		return m_boot_result;
	}

	// 1. Initialize OpenOrbis Subsystem Hub
	m_subsystem_hub = std::make_unique<Kernel::OpenOrbisSubsystemHub>(m_telemetry);
	m_subsystem_hub->RegisterAll();

	// 2. Initialize Darwin Mach Exception Server
	m_mach_handler = std::make_unique<Kernel::MachExceptionHandler>();
	m_mach_handler->Initialize();

	// 3. Initialize Metal Graphics Backend
	m_metal_backend = std::make_unique<Libs::Graphics::MetalGraphicBackend>();
	bool metal_ok = m_metal_backend->Initialize();
	(void)metal_ok;

	// 4. Load binary through OpenOrbis ELF loader
	m_loader = std::make_unique<Loader::OpenOrbisElfLoader>();
	auto resolver = m_subsystem_hub->CreateSymbolResolver(0x80000000);

	Loader::OpenOrbisLoadResult result{};

	if (m_boot_result.detected_format == ExecutableFormat::Elf) {
		result = m_loader->Load(file_path, resolver);
	} else if (m_boot_result.detected_format == ExecutableFormat::Self) {
		std::ifstream in(file_path, std::ios::binary | std::ios::ate);
		if (in) {
			size_t size = in.tellg();
			in.seekg(0, std::ios::beg);
			std::vector<uint8_t> self_data(size);
			in.read(reinterpret_cast<char*>(self_data.data()), size);
			std::vector<uint8_t> extracted_elf;
			if (Loader::SelfParser::ExtractElf(self_data.data(), self_data.size(), extracted_elf)) {
				result = m_loader->LoadFromMemory(extracted_elf.data(), extracted_elf.size(), file_path, resolver);
			} else {
				result = m_loader->Load(file_path, resolver);
			}
		}
	} else if (m_boot_result.detected_format == ExecutableFormat::Pkg) {
		std::ifstream in(file_path, std::ios::binary | std::ios::ate);
		if (in) {
			size_t size = in.tellg();
			in.seekg(0, std::ios::beg);
			std::vector<uint8_t> pkg_data(size);
			in.read(reinterpret_cast<char*>(pkg_data.data()), size);

			char temp_dir[] = "/tmp/kyty_pkg_XXXXXX";
			if (mkdtemp(temp_dir) != nullptr) {
				std::string content_id;
				if (Libs::Kernel::Ps5::PkgInstaller::InstallPackageBuffer(pkg_data.data(), pkg_data.size(), temp_dir, &content_id)) {
					std::filesystem::path eboot_path = std::filesystem::path(temp_dir) / "eboot.bin";
					if (std::filesystem::exists(eboot_path)) {
						auto eboot_fmt = DetectFormat(eboot_path.string());
						if (eboot_fmt == ExecutableFormat::Self) {
							std::ifstream eb_in(eboot_path, std::ios::binary | std::ios::ate);
							if (eb_in) {
								size_t eb_size = eb_in.tellg();
								eb_in.seekg(0, std::ios::beg);
								std::vector<uint8_t> eb_data(eb_size);
								eb_in.read(reinterpret_cast<char*>(eb_data.data()), eb_size);
								std::vector<uint8_t> extracted_elf;
								if (Loader::SelfParser::ExtractElf(eb_data.data(), eb_data.size(), extracted_elf)) {
									result = m_loader->LoadFromMemory(extracted_elf.data(), extracted_elf.size(), eboot_path.string(), resolver);
								}
							}
						} else {
							result = m_loader->Load(eboot_path, resolver);
						}
					}
				}
			}
		}
	}

	if (!result.success) {
		m_boot_result.error_message = result.error_message.empty() ? "Failed to load and relocate binary" : result.error_message;
		return m_boot_result;
	}

	m_boot_result.success           = result.success;
	m_boot_result.entry_point_vaddr = result.entry_vaddr;
	m_boot_result.image_base_vaddr  = result.base_vaddr;
	m_boot_result.symbols_resolved  = result.resolved_symbols_count;

	m_booted = true;

	if (run_main_loop) {
		RunFrames(1);
	}

	return m_boot_result;
}

bool GameBootOrchestrator::RunFrames(uint32_t frame_count) {
	if (!m_booted || !m_metal_backend) return false;

	for (uint32_t i = 0; i < frame_count; ++i) {
		m_metal_backend->PresentFrame(i);
	}

	return true;
}

void GameBootOrchestrator::Shutdown() {
	if (m_booted) {
		if (m_metal_backend) {
			m_metal_backend->Shutdown();
			m_metal_backend.reset();
		}
		if (m_mach_handler) {
			m_mach_handler->Shutdown();
			m_mach_handler.reset();
		}
		m_loader.reset();
		m_subsystem_hub.reset();
		m_booted = false;
	}
}

} // namespace Emulator
