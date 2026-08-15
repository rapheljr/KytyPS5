// saveStateEngine.h
//
// Save State Serialization & Snapshot Engine for Phase O Full Integration.

#ifndef EMULATOR_SAVE_STATE_ENGINE_H
#define EMULATOR_SAVE_STATE_ENGINE_H

#include "common/common.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Emulator {

struct SaveStateHeader {
	uint32_t magic                 = 0x4B595459; // "KYTY"
	uint32_t version               = 2;
	uint64_t timestamp             = 0;
	uint64_t frames_count          = 0;
	uint32_t context_size          = sizeof(Loader::Recompiler::GuestCpuContext);
	uint32_t extra_data_size       = 0;
	uint32_t compressed_extra_size = 0;
};

struct SaveStateSnapshot {
	SaveStateHeader                      header;
	Loader::Recompiler::GuestCpuContext cpu_context;
	std::vector<uint8_t>                 extra_data;
};

class SaveStateEngine {
public:
	SaveStateEngine() = default;
	~SaveStateEngine() = default;

	KYTY_CLASS_NO_COPY(SaveStateEngine);

	static bool CreateSnapshot(const Loader::Recompiler::GuestCpuContext& ctx, uint64_t frames_count, SaveStateSnapshot& out_snapshot);
	static bool SaveToFile(const SaveStateSnapshot& snapshot, const std::string& filepath);
	static bool LoadFromFile(const std::string& filepath, SaveStateSnapshot& out_snapshot);
	static bool RestoreSnapshot(const SaveStateSnapshot& snapshot, Loader::Recompiler::GuestCpuContext& out_ctx);
};

} // namespace Emulator

#endif // EMULATOR_SAVE_STATE_ENGINE_H
