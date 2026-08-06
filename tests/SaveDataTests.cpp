// SaveDataTests.cpp
//
// Unit & Integration Tests for SaveData Directory Initialization and Slot Allocation.

#include "common/file.h"
#include "libs/saveDataMountSlots.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

using namespace Libs::SaveData;

static void TestSaveDataMountSlots() {
	std::printf("[TEST] SaveDataMountSlotsOperations\n");

	SaveDataMountSlots slots;

	if (!slots.Empty()) {
		std::fprintf(stderr, "FAIL: Newly created slots container is not empty\n");
		std::exit(1);
	}

	int slot0 = slots.FindAvailable("SAVE_SLOT_01");
	if (slot0 < 0) {
		std::fprintf(stderr, "FAIL: FindAvailable returned invalid slot: %d\n", slot0);
		std::exit(1);
	}

	slots.Mount(static_cast<size_t>(slot0), "SAVE_SLOT_01");

	// Duplicate mount should yield BUSY (-2)
	int dup_slot = slots.FindAvailable("SAVE_SLOT_01");
	if (dup_slot != SaveDataMountSlots::BUSY) {
		std::fprintf(stderr, "FAIL: Expected BUSY (-2) for duplicate mount, got %d\n", dup_slot);
		std::exit(1);
	}

	std::string mp0 = SaveDataMountSlots::MountPoint(static_cast<size_t>(slot0));
	if (mp0 != "/savedata0") {
		std::fprintf(stderr, "FAIL: Unexpected mount point string: %s\n", mp0.c_str());
		std::exit(1);
	}

	int found_slot = slots.Find("/savedata0");
	if (found_slot != slot0) {
		std::fprintf(stderr, "FAIL: Find by mount point failed: expected %d, got %d\n", slot0, found_slot);
		std::exit(1);
	}

	slots.Release(static_cast<size_t>(slot0));
	if (!slots.Empty()) {
		std::fprintf(stderr, "FAIL: Slots container not empty after release\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SaveDataMountSlotsOperations\n");
}

static void TestInitDirectoryCreation() {
	std::printf("[TEST] SaveDataInitDirectoryCreation\n");

	std::string test_dir = "_SaveData/PPSA01234/slot_test";
	Common::File::CreateDirectories(test_dir);

	if (!Common::File::IsDirectoryExisting(test_dir)) {
		std::fprintf(stderr, "FAIL: SaveData directory was not created: %s\n", test_dir.c_str());
		std::exit(1);
	}

	std::error_code ec;
	std::filesystem::remove_all("_SaveData", ec);

	std::printf("  [ OK ] SaveDataInitDirectoryCreation\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — SaveData Directory & Mount Slot Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestSaveDataMountSlots();
	TestInitDirectoryCreation();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed\n");
	std::printf("================================================================================\n");

	return 0;
}
