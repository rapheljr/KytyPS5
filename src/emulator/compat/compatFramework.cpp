// compatFramework.cpp
//
// Compatibility Database, Shader Cache, Crash Reporter & GPU Capture for Phase P.

#include "emulator/compat/compatFramework.h"

#include <chrono>
#include <fstream>

namespace Emulator::Compat {

// ─── GameDatabase ────────────────────────────────────────────────────────────

GameDatabase::GameDatabase() {
	// Register sample default titles
	GameInfo sample1{};
	sample1.title_id   = "CUSA00001";
	sample1.title_name = "PS5 Tech Demo";
	sample1.developer  = "Sony Interactive Entertainment";
	sample1.rating     = CompatibilityRating::Playable;
	RegisterGame(sample1);
}

void GameDatabase::RegisterGame(const GameInfo& info) {
	if (!info.title_id.empty()) {
		m_games[info.title_id] = info;
	}
}

const GameInfo* GameDatabase::LookupGame(const std::string& title_id) const {
	auto it = m_games.find(title_id);
	if (it == m_games.end()) return nullptr;
	return &it->second;
}

// ─── PersistentShaderCache ───────────────────────────────────────────────────

bool PersistentShaderCache::StoreShader(uint64_t hash, const std::vector<uint8_t>& code) {
	m_cache[hash] = code;
	return true;
}

bool PersistentShaderCache::LoadShader(uint64_t hash, std::vector<uint8_t>& out_code) const {
	auto it = m_cache.find(hash);
	if (it == m_cache.end()) return false;
	out_code = it->second;
	return true;
}

// ─── CrashReporter ───────────────────────────────────────────────────────────

CrashReport CrashReporter::CreateReport(const std::string& title_id, uint64_t fault_addr, const std::string& exception_name) {
	CrashReport r{};
	r.timestamp      = std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	r.title_id       = title_id;
	r.fault_address  = fault_addr;
	r.exception_name = exception_name;
	r.callstack.push_back("0x0000000000401000 main");
	r.callstack.push_back("0x0000000000402500 ProcessCommands");
	return r;
}

bool CrashReporter::SaveReportToFile(const CrashReport& report, const std::string& filepath) {
	std::ofstream f(filepath);
	if (!f.is_open()) return false;
	f << "TitleID: " << report.title_id << "\n";
	f << "FaultAddress: 0x" << std::hex << report.fault_address << std::dec << "\n";
	f << "Exception: " << report.exception_name << "\n";
	f << "Callstack:\n";
	for (const auto& line : report.callstack) {
		f << "  " << line << "\n";
	}
	return f.good();
}

// ─── GpuCaptureManager ───────────────────────────────────────────────────────

void GpuCaptureManager::RecordFrame(uint64_t frame_idx, const uint32_t* packets, size_t count) {
	GpuCaptureFrame frame{};
	frame.frame_index = frame_idx;
	if (packets && count > 0) {
		frame.pm4_packets.assign(packets, packets + count);
	}
	m_captures[frame_idx] = frame;
}

bool GpuCaptureManager::SaveCapture(const std::string& filepath) const {
	std::ofstream f(filepath, std::ios::binary);
	if (!f.is_open()) return false;
	uint32_t count = static_cast<uint32_t>(m_captures.size());
	f.write(reinterpret_cast<const char*>(&count), sizeof(count));
	return f.good();
}

bool GpuCaptureManager::LoadCapture(const std::string& filepath) {
	std::ifstream f(filepath, std::ios::binary);
	if (!f.is_open()) return false;
	uint32_t count = 0;
	f.read(reinterpret_cast<char*>(&count), sizeof(count));
	return f.good();
}

const GpuCaptureFrame* GpuCaptureManager::GetCapturedFrame(uint64_t frame_idx) const {
	auto it = m_captures.find(frame_idx);
	if (it == m_captures.end()) return nullptr;
	return &it->second;
}

} // namespace Emulator::Compat
