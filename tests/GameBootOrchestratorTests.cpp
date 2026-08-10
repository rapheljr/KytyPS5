// GameBootOrchestratorTests.cpp
//
// Unit & Integration Tests for Direct Commercial Game Boot Orchestrator & CLI Runner.

#include "emulator/gameBootOrchestrator.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <vector>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Emulator;

void TestFormatDetection() {
	std::printf("[TEST] GameBootOrchestrator_FormatDetection\n");

	// Create temporary mock ELF file
	std::string elf_path = "_test_mock.elf";
	{
		std::ofstream out(elf_path, std::ios::binary);
		uint8_t elf_magic[4] = { 0x7F, 'E', 'L', 'F' };
		out.write(reinterpret_cast<const char*>(elf_magic), 4);
	}

	// Create temporary mock SELF file
	std::string self_path = "_test_mock.self";
	{
		std::ofstream out(self_path, std::ios::binary);
		uint8_t self_magic[4] = { 'S', 'C', 'E', 0x00 };
		out.write(reinterpret_cast<const char*>(self_magic), 4);
	}

	// Create temporary mock PKG file
	std::string pkg_path = "_test_mock.pkg";
	{
		std::ofstream out(pkg_path, std::ios::binary);
		uint8_t pkg_magic[4] = { 0x7F, 'C', 'N', 'T' };
		out.write(reinterpret_cast<const char*>(pkg_magic), 4);
	}

	Check(GameBootOrchestrator::DetectFormat(elf_path) == ExecutableFormat::Elf, "DetectFormat ELF mismatch");
	Check(GameBootOrchestrator::DetectFormat(self_path) == ExecutableFormat::Self, "DetectFormat SELF mismatch");
	Check(GameBootOrchestrator::DetectFormat(pkg_path) == ExecutableFormat::Pkg, "DetectFormat PKG mismatch");
	Check(GameBootOrchestrator::DetectFormat("_non_existent_file.bin") == ExecutableFormat::Unknown, "Unknown format mismatch");

	std::remove(elf_path.c_str());
	std::remove(self_path.c_str());
	std::remove(pkg_path.c_str());

	std::printf("  [ OK ] GameBootOrchestrator_FormatDetection\n");
}

void TestGameBootLifecycle() {
	std::printf("[TEST] GameBootOrchestrator_Lifecycle\n");

	GameBootOrchestrator orchestrator;
	Check(!orchestrator.IsBooted(), "Orchestrator should not be booted initially");

	// Boot non-existent file
	BootResult bad_res = orchestrator.BootExecutable("_invalid_path.elf");
	Check(!bad_res.success, "BootExecutable on invalid path should fail");

	orchestrator.Shutdown();
	Check(!orchestrator.IsBooted(), "Orchestrator should be shut down");

	std::printf("  [ OK ] GameBootOrchestrator_Lifecycle\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Commercial Game Boot Orchestrator & CLI Runner Test Suite\n");
	std::printf("================================================================================\n");

	TestFormatDetection();
	TestGameBootLifecycle();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
