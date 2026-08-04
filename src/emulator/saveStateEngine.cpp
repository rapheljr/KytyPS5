// saveStateEngine.cpp
//
// Save State Serialization & Snapshot Engine for Phase O Full Integration.

#include "emulator/saveStateEngine.h"

#include <chrono>
#include <fstream>

namespace Emulator {

bool SaveStateEngine::CreateSnapshot(const Loader::Recompiler::GuestCpuContext& ctx, uint64_t frames_count, SaveStateSnapshot& out_snapshot) {
	out_snapshot.header.magic        = 0x4B595459;
	out_snapshot.header.version      = 1;
	out_snapshot.header.timestamp    = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	out_snapshot.header.frames_count = frames_count;
	out_snapshot.header.context_size = sizeof(Loader::Recompiler::GuestCpuContext);

	out_snapshot.cpu_context = ctx;
	return true;
}

bool SaveStateEngine::SaveToFile(const SaveStateSnapshot& snapshot, const std::string& filepath) {
	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open()) return false;

	file.write(reinterpret_cast<const char*>(&snapshot.header), sizeof(SaveStateHeader));
	file.write(reinterpret_cast<const char*>(&snapshot.cpu_context), sizeof(Loader::Recompiler::GuestCpuContext));
	return file.good();
}

bool SaveStateEngine::LoadFromFile(const std::string& filepath, SaveStateSnapshot& out_snapshot) {
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open()) return false;

	file.read(reinterpret_cast<char*>(&out_snapshot.header), sizeof(SaveStateHeader));
	if (out_snapshot.header.magic != 0x4B595459) return false;

	file.read(reinterpret_cast<char*>(&out_snapshot.cpu_context), sizeof(Loader::Recompiler::GuestCpuContext));
	return file.good();
}

bool SaveStateEngine::RestoreSnapshot(const SaveStateSnapshot& snapshot, Loader::Recompiler::GuestCpuContext& out_ctx) {
	if (snapshot.header.magic != 0x4B595459) return false;
	out_ctx = snapshot.cpu_context;
	return true;
}

} // namespace Emulator
