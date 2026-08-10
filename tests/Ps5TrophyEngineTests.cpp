// Ps5TrophyEngineTests.cpp
//
// Unit & Integration Tests for PS5 Trophies & PSN Mock Subsystem.

#include "compat/ps5TrophyEngine.h"

#include <cstdio>
#include <cstdlib>

using namespace Compat;

static void TestTrophyRegistrationAndUnlock() {
	std::printf("[TEST] TrophyEngine_RegistrationAndUnlock\n");

	Ps5TrophyEngine engine;

	engine.RegisterTrophy(1, TrophyGrade::Bronze, "First Steps", "Completed Level 1");
	engine.RegisterTrophy(2, TrophyGrade::Platinum, "Master of Kyty", "Collected all trophies");

	TrophyDefinition def{};
	if (!engine.GetTrophy(1, def) || def.is_unlocked) {
		std::fprintf(stderr, "FAIL: Trophy 1 registered incorrectly\n");
		std::exit(1);
	}

	// Unlock Trophy 1
	if (!engine.UnlockTrophy(1, 1700000000ULL)) {
		std::fprintf(stderr, "FAIL: UnlockTrophy 1 failed\n");
		std::exit(1);
	}

	engine.GetTrophy(1, def);
	if (!def.is_unlocked || def.unlock_timestamp != 1700000000ULL) {
		std::fprintf(stderr, "FAIL: Trophy 1 unlock state mismatch\n");
		std::exit(1);
	}

	// Re-unlocking should fail
	if (engine.UnlockTrophy(1)) {
		std::fprintf(stderr, "FAIL: Re-unlocking already unlocked trophy succeeded\n");
		std::exit(1);
	}

	const auto& stats = engine.GetStats();
	if (stats.total_registered != 2 || stats.total_unlocked != 1 || stats.bronze_unlocked != 1) {
		std::fprintf(stderr, "FAIL: Trophy stats mismatch (Reg=%u, Unlocked=%u)\n", stats.total_registered, stats.total_unlocked);
		std::exit(1);
	}

	std::printf("  [ OK ] TrophyEngine_RegistrationAndUnlock\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 Trophy Engine Test Suite\n");
	std::printf("================================================================================\n");

	TestTrophyRegistrationAndUnlock();

	std::printf("================================================================================\n");
	std::printf("  Results: 1 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
