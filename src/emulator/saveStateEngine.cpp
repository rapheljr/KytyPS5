#include "emulator/saveStateEngine.h"

#include <zlib.h>
#include <chrono>
#include <fstream>
#include <vector>

namespace Emulator {

bool SaveStateEngine::CreateSnapshot(const Loader::Recompiler::GuestCpuContext& ctx, uint64_t frames_count, SaveStateSnapshot& out_snapshot) {
	out_snapshot.header.magic        = 0x4B595459;
	out_snapshot.header.version      = 2;
	out_snapshot.header.timestamp    = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	out_snapshot.header.frames_count = frames_count;
	out_snapshot.header.context_size = sizeof(Loader::Recompiler::GuestCpuContext);
	out_snapshot.header.extra_data_size = static_cast<uint32_t>(out_snapshot.extra_data.size());
	out_snapshot.header.compressed_extra_size = 0;

	out_snapshot.cpu_context = ctx;
	return true;
}

bool SaveStateEngine::SaveToFile(const SaveStateSnapshot& snapshot, const std::string& filepath) {
	std::ofstream file(filepath, std::ios::binary);
	if (!file.is_open()) return false;

	SaveStateSnapshot mutable_snap = snapshot;
	std::vector<uint8_t> compressed_extra;

	if (!mutable_snap.extra_data.empty()) {
		uLongf bound = compressBound(static_cast<uLong>(mutable_snap.extra_data.size()));
		compressed_extra.resize(bound);
		uLongf compressed_size = bound;
		int res = compress(compressed_extra.data(), &compressed_size,
		                   mutable_snap.extra_data.data(), static_cast<uLong>(mutable_snap.extra_data.size()));
		if (res == Z_OK) {
			compressed_extra.resize(compressed_size);
			mutable_snap.header.extra_data_size = static_cast<uint32_t>(mutable_snap.extra_data.size());
			mutable_snap.header.compressed_extra_size = static_cast<uint32_t>(compressed_size);
		} else {
			mutable_snap.header.extra_data_size = 0;
			mutable_snap.header.compressed_extra_size = 0;
		}
	}

	file.write(reinterpret_cast<const char*>(&mutable_snap.header), sizeof(SaveStateHeader));
	file.write(reinterpret_cast<const char*>(&mutable_snap.cpu_context), sizeof(Loader::Recompiler::GuestCpuContext));
	if (mutable_snap.header.compressed_extra_size > 0) {
		file.write(reinterpret_cast<const char*>(compressed_extra.data()), mutable_snap.header.compressed_extra_size);
	}
	return file.good();
}

bool SaveStateEngine::LoadFromFile(const std::string& filepath, SaveStateSnapshot& out_snapshot) {
	std::ifstream file(filepath, std::ios::binary);
	if (!file.is_open()) return false;

	file.read(reinterpret_cast<char*>(&out_snapshot.header), sizeof(SaveStateHeader));
	if (out_snapshot.header.magic != 0x4B595459) return false;

	file.read(reinterpret_cast<char*>(&out_snapshot.cpu_context), sizeof(Loader::Recompiler::GuestCpuContext));

	if (out_snapshot.header.version >= 2 && out_snapshot.header.compressed_extra_size > 0) {
		std::vector<uint8_t> compressed(out_snapshot.header.compressed_extra_size);
		file.read(reinterpret_cast<char*>(compressed.data()), out_snapshot.header.compressed_extra_size);

		out_snapshot.extra_data.resize(out_snapshot.header.extra_data_size);
		uLongf uncomp_size = out_snapshot.header.extra_data_size;
		int res = uncompress(out_snapshot.extra_data.data(), &uncomp_size,
		                     compressed.data(), static_cast<uLong>(compressed.size()));
		if (res != Z_OK) {
			out_snapshot.extra_data.clear();
		}
	} else {
		out_snapshot.extra_data.clear();
	}

	return file.good();
}

bool SaveStateEngine::RestoreSnapshot(const SaveStateSnapshot& snapshot, Loader::Recompiler::GuestCpuContext& out_ctx) {
	if (snapshot.header.magic != 0x4B595459) return false;
	out_ctx = snapshot.cpu_context;
	return true;
}

} // namespace Emulator
