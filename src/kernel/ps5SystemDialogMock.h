// ps5SystemDialogMock.h
//
// PS5 System Common Dialogs & IME / OSK Mock Subsystem for KytyPS5.
// Emulates sceCommonDialog, sceMsgDialog, sceImeDialog (OSK), and sceSaveDataDialog.

#ifndef KERNEL_PS5_SYSTEM_DIALOG_MOCK_H
#define KERNEL_PS5_SYSTEM_DIALOG_MOCK_H

#include "common/common.h"

#include <cstdint>
#include <mutex>
#include <string>

namespace Kernel {

enum class DialogType : uint8_t {
	None = 0,
	MsgDialog,
	ImeDialog,       // On-Screen Keyboard
	SaveDataDialog
};

enum class DialogStatus : uint8_t {
	None = 0,
	Running,
	Finished,
	Canceled
};

enum class DialogButtonResult : uint8_t {
	Ok = 0,
	Yes,
	No,
	Cancel
};

struct DialogStats {
	uint32_t total_dialogs_opened = 0;
	uint32_t total_dialogs_closed = 0;
};

class Ps5SystemDialogMock {
public:
	Ps5SystemDialogMock();
	~Ps5SystemDialogMock() = default;

	KYTY_CLASS_NO_COPY(Ps5SystemDialogMock);

	/// Open a Message Dialog (sceMsgDialog)
	bool ShowMsgDialog(const std::string& message, DialogButtonResult auto_response = DialogButtonResult::Ok);

	/// Open an IME / On-Screen Keyboard Dialog (sceImeDialog)
	bool ShowImeDialog(const std::string& title, const std::string& initial_text, const std::string& mock_input_text = "Player1");

	/// Update dialog state machine (simulates frame advance)
	void Update();

	/// Close current active dialog
	void CloseDialog();

	[[nodiscard]] DialogType GetActiveDialogType() const noexcept { return m_active_type; }
	[[nodiscard]] DialogStatus GetStatus() const noexcept { return m_status; }
	[[nodiscard]] DialogButtonResult GetButtonResult() const noexcept { return m_button_result; }
	[[nodiscard]] const std::string& GetImeResultText() const noexcept { return m_ime_result; }
	[[nodiscard]] const DialogStats& GetStats() const noexcept { return m_stats; }

	void Reset() noexcept;

private:
	DialogType         m_active_type   = DialogType::None;
	DialogStatus       m_status        = DialogStatus::None;
	DialogButtonResult m_button_result = DialogButtonResult::Ok;
	std::string        m_ime_result;
	std::mutex         m_mutex;
	DialogStats        m_stats{};
};

} // namespace Kernel

#endif // KERNEL_PS5_SYSTEM_DIALOG_MOCK_H
