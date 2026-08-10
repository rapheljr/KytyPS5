// Ps5SystemDialogMockTests.cpp
//
// Unit & Integration Tests for PS5 System Dialogs & IME / OSK Mock Subsystem.

#include "kernel/ps5SystemDialogMock.h"

#include <cstdio>
#include <cstdlib>

using namespace Kernel;

static void TestMsgDialogFlow() {
	std::printf("[TEST] SystemDialog_MsgDialogFlow\n");

	Ps5SystemDialogMock dialogs;

	if (!dialogs.ShowMsgDialog("Press OK to continue", DialogButtonResult::Ok)) {
		std::fprintf(stderr, "FAIL: ShowMsgDialog failed\n");
		std::exit(1);
	}

	if (dialogs.GetStatus() != DialogStatus::Running) {
		std::fprintf(stderr, "FAIL: Dialog status not Running\n");
		std::exit(1);
	}

	// Advance frame
	dialogs.Update();
	if (dialogs.GetStatus() != DialogStatus::Finished) {
		std::fprintf(stderr, "FAIL: Dialog status not Finished after update\n");
		std::exit(1);
	}

	if (dialogs.GetButtonResult() != DialogButtonResult::Ok) {
		std::fprintf(stderr, "FAIL: Button result mismatch\n");
		std::exit(1);
	}

	dialogs.CloseDialog();
	if (dialogs.GetStatus() != DialogStatus::None) {
		std::fprintf(stderr, "FAIL: Dialog status not None after close\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SystemDialog_MsgDialogFlow\n");
}

static void TestImeDialogFlow() {
	std::printf("[TEST] SystemDialog_ImeDialogFlow\n");

	Ps5SystemDialogMock dialogs;

	if (!dialogs.ShowImeDialog("Enter Character Name", "Hero", "CloudStrife")) {
		std::fprintf(stderr, "FAIL: ShowImeDialog failed\n");
		std::exit(1);
	}

	dialogs.Update();
	if (dialogs.GetStatus() != DialogStatus::Finished) {
		std::fprintf(stderr, "FAIL: IME Dialog not finished after update\n");
		std::exit(1);
	}

	if (dialogs.GetImeResultText() != "CloudStrife") {
		std::fprintf(stderr, "FAIL: IME result text mismatch: %s\n", dialogs.GetImeResultText().c_str());
		std::exit(1);
	}

	dialogs.CloseDialog();
	const auto& stats = dialogs.GetStats();
	if (stats.total_dialogs_opened != 1 || stats.total_dialogs_closed != 1) {
		std::fprintf(stderr, "FAIL: Dialog stats mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] SystemDialog_ImeDialogFlow\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 System Dialogs & OSK Mock Test Suite\n");
	std::printf("================================================================================\n");

	TestMsgDialogFlow();
	TestImeDialogFlow();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
