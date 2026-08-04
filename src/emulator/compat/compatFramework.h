// compatFramework.h
//
// Compatibility Database, Shader Cache, Crash Reporter & GPU Capture for Phase P.

#ifndef EMULATOR_COMPAT_COMPAT_FRAMEWORK_H
#define EMULATOR_COMPAT_COMPAT_FRAMEWORK_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Emulator::Compat {

enum class CompatibilityRating : uint8_t {
	Nothing = 0,
	Bootable,
	Intro,
	Ingame,
	Playable,
	Perfect
};

struct GameInfo {
	std::string         title_id; // e.g. "CUSA00001"
	std::string         title_name;
	std::string         developer;
	CompatibilityRating rating = CompatibilityRating::Nothing;
	std::unordered_map<std::string, std::string> config_overrides;
};

class GameDatabase {
public:
	GameDatabase();
	~GameDatabase() = default;

	KYTY_CLASS_NO_COPY(GameDatabase);

	void RegisterGame(const GameInfo& info);
	[[nodiscard]] const GameInfo* LookupGame(const std::string& title_id) const;
	[[nodiscard]] size_t GetGameCount() const noexcept { return m_games.size(); }

private:
	std::unordered_map<std::string, GameInfo> m_games;
};

class PersistentShaderCache {
public:
	explicit PersistentShaderCache(const std::string& cache_dir = "/tmp/kyty_shader_cache")
	    : m_cache_dir(cache_dir) {}
	~PersistentShaderCache() = default;

	KYTY_CLASS_NO_COPY(PersistentShaderCache);

	bool StoreShader(uint64_t hash, const std::vector<uint8_t>& code);
	bool LoadShader(uint64_t hash, std::vector<uint8_t>& out_code) const;
	[[nodiscard]] size_t GetCachedCount() const noexcept { return m_cache.size(); }

private:
	std::string m_cache_dir;
	mutable std::unordered_map<uint64_t, std::vector<uint8_t>> m_cache;
};

struct CrashReport {
	uint64_t    timestamp = 0;
	std::string title_id;
	uint64_t    fault_address = 0;
	std::string exception_name;
	std::vector<std::string> callstack;
};

class CrashReporter {
public:
	CrashReporter() = default;
	~CrashReporter() = default;

	KYTY_CLASS_NO_COPY(CrashReporter);

	static CrashReport CreateReport(const std::string& title_id, uint64_t fault_addr, const std::string& exception_name);
	static bool SaveReportToFile(const CrashReport& report, const std::string& filepath);
};

struct GpuCaptureFrame {
	uint64_t frame_index = 0;
	std::vector<uint32_t> pm4_packets;
};

class GpuCaptureManager {
public:
	GpuCaptureManager() = default;
	~GpuCaptureManager() = default;

	KYTY_CLASS_NO_COPY(GpuCaptureManager);

	void RecordFrame(uint64_t frame_idx, const uint32_t* packets, size_t count);
	bool SaveCapture(const std::string& filepath) const;
	bool LoadCapture(const std::string& filepath);
	[[nodiscard]] const GpuCaptureFrame* GetCapturedFrame(uint64_t frame_idx) const;

private:
	std::unordered_map<uint64_t, GpuCaptureFrame> m_captures;
};

} // namespace Emulator::Compat

#endif // EMULATOR_COMPAT_COMPAT_FRAMEWORK_H
