// Ps5SystemServicesTests.cpp
//
// Unit & Integration tests for PS5 System Services (UserService, SaveData & Sealed Keystore).

#include "libs/errno.h"
#include "libs/saveDataMountSlots.h"
#include "loader/ps5SaveDataSealedKeystore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Libs::SaveData;
using namespace Loader;

void TestSaveDataMountSlots() {
	std::printf("[TEST] PS5 SaveData Mount Slots & Sandboxing...\n");

	SaveDataMountSlots slots;
	Check(slots.Empty(), "Slots container should be empty initially");

	int slot0 = slots.FindAvailable("SAVE_DIR_GAME01");
	Check(slot0 >= 0, "Available slot expected");

	slots.Mount(static_cast<size_t>(slot0), "SAVE_DIR_GAME01");

	// Duplicate mount should yield BUSY (-2)
	int dup_slot = slots.FindAvailable("SAVE_DIR_GAME01");
	Check(dup_slot == SaveDataMountSlots::BUSY, "Expected BUSY for duplicate mount");

	std::string mp0 = SaveDataMountSlots::MountPoint(static_cast<size_t>(slot0));
	Check(mp0 == "/savedata0", "Unexpected mount point format");

	int found_slot = slots.Find("/savedata0");
	Check(found_slot == slot0, "Find by mount point failed");

	slots.Release(static_cast<size_t>(slot0));
	Check(slots.Empty(), "Slots container should be empty after release");

	std::printf("  [OK] PS5 SaveData Mount Slots & Sandboxing\n");
}

void TestSealedKeystore() {
	std::printf("[TEST] PS5 Sealed Keystore & Security Boundaries...\n");

	Ps5SaveDataSealedKeystore keystore;
	std::string title_id = "PPSA01234";
	uint64_t account_id = 0x20000000ULL;

	std::vector<uint8_t> original_key = {
		0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44,
		0x55, 0x66, 0x77, 0x88, 0x99, 0x00, 0xEE, 0xFF,
		0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
		0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
	};

	auto sealed = keystore.SealKey(title_id, account_id, original_key.data(), original_key.size());
	Check(!sealed.empty(), "Sealed key payload empty");

	std::vector<uint8_t> unsealed;
	bool ok = keystore.UnsealKey(sealed.data(), sealed.size(), title_id, account_id, unsealed);
	Check(ok && unsealed == original_key, "Unsealed key mismatch");

	std::vector<uint8_t> mismatched;
	bool fail_ok = keystore.UnsealKey(sealed.data(), sealed.size(), "PPSA99999", account_id, mismatched);
	Check(!fail_ok, "UnsealKey should fail for mismatched title ID");

	std::printf("  [OK] PS5 Sealed Keystore & Security Boundaries\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — System Services (SaveData & Security) Test Suite\n");
	std::printf("================================================================================\n");

	TestSaveDataMountSlots();
	TestSealedKeystore();

	std::printf("================================================================================\n");
	std::printf("  Results: All System Services Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
