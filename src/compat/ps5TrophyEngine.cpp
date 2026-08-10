// ps5TrophyEngine.cpp
//
// PS5 Trophies & PlayStation Network (PSN) Mock Subsystem Implementation.

#include "compat/ps5TrophyEngine.h"

#include <chrono>

namespace Compat {

Ps5TrophyEngine::Ps5TrophyEngine() = default;

bool Ps5TrophyEngine::RegisterTrophy(uint32_t id, TrophyGrade grade, const std::string& name, const std::string& desc) {
	std::lock_guard<std::mutex> lock(m_mutex);

	TrophyDefinition def;
	def.id          = id;
	def.grade       = grade;
	def.name        = name;
	def.description = desc;
	def.is_unlocked = false;

	m_trophies[id] = std::move(def);
	m_stats.total_registered = static_cast<uint32_t>(m_trophies.size());

	return true;
}

bool Ps5TrophyEngine::UnlockTrophy(uint32_t id, uint64_t timestamp) {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_trophies.find(id);
	if (it == m_trophies.end() || it->second.is_unlocked) return false;

	it->second.is_unlocked = true;
	it->second.unlock_timestamp = (timestamp != 0) ? timestamp :
		static_cast<uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());

	m_stats.total_unlocked++;
	switch (it->second.grade) {
		case TrophyGrade::Bronze:   m_stats.bronze_unlocked++; break;
		case TrophyGrade::Silver:   m_stats.silver_unlocked++; break;
		case TrophyGrade::Gold:     m_stats.gold_unlocked++; break;
		case TrophyGrade::Platinum: m_stats.platinum_unlocked++; break;
	}

	return true;
}

bool Ps5TrophyEngine::GetTrophy(uint32_t id, TrophyDefinition& out_def) const {
	std::lock_guard<std::mutex> lock(m_mutex);

	auto it = m_trophies.find(id);
	if (it != m_trophies.end()) {
		out_def = it->second;
		return true;
	}

	return false;
}

std::vector<TrophyDefinition> Ps5TrophyEngine::GetAllTrophies() const {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::vector<TrophyDefinition> result;
	result.reserve(m_trophies.size());

	for (const auto& [id, def] : m_trophies) {
		result.push_back(def);
	}

	return result;
}

void Ps5TrophyEngine::Reset() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_trophies.clear();
	m_stats = {};
}

} // namespace Compat
