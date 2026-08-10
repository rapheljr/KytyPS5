// ps5TrophyEngine.h
//
// PS5 Trophies & PlayStation Network (PSN) Mock Subsystem for KytyPS5.
// Emulates sceNpTrophy registration, unlock states, and local progression persistence.

#ifndef COMPAT_PS5_TROPHY_ENGINE_H
#define COMPAT_PS5_TROPHY_ENGINE_H

#include "common/common.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Compat {

enum class TrophyGrade : uint8_t {
	Bronze = 0,
	Silver,
	Gold,
	Platinum
};

struct TrophyDefinition {
	uint32_t    id               = 0;
	TrophyGrade grade            = TrophyGrade::Bronze;
	std::string name;
	std::string description;
	bool        is_unlocked      = false;
	uint64_t    unlock_timestamp = 0;
};

struct TrophyStats {
	uint32_t total_registered = 0;
	uint32_t total_unlocked   = 0;
	uint32_t bronze_unlocked  = 0;
	uint32_t silver_unlocked  = 0;
	uint32_t gold_unlocked    = 0;
	uint32_t platinum_unlocked= 0;
};

class Ps5TrophyEngine {
public:
	Ps5TrophyEngine();
	~Ps5TrophyEngine() = default;

	KYTY_CLASS_NO_COPY(Ps5TrophyEngine);

	/// Register a trophy definition from game metadata (TRP / XML)
	bool RegisterTrophy(uint32_t id, TrophyGrade grade, const std::string& name, const std::string& desc);

	/// Unlock a trophy (sceNpTrophyUnlock)
	bool UnlockTrophy(uint32_t id, uint64_t timestamp = 0);

	/// Query trophy state
	bool GetTrophy(uint32_t id, TrophyDefinition& out_def) const;

	/// Get all trophies
	std::vector<TrophyDefinition> GetAllTrophies() const;

	[[nodiscard]] const TrophyStats& GetStats() const noexcept { return m_stats; }
	void Reset() noexcept;

private:
	std::map<uint32_t, TrophyDefinition> m_trophies;
	mutable std::mutex                   m_mutex;
	TrophyStats                          m_stats{};
};

} // namespace Compat

#endif // COMPAT_PS5_TROPHY_ENGINE_H
