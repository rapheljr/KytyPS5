// ps5SystemDialogMock.cpp
//
// PS5 System Common Dialogs & IME / OSK Implementation.

#include "kernel/ps5SystemDialogMock.h"

namespace Kernel {

Ps5SystemDialogMock::Ps5SystemDialogMock() = default;

bool Ps5SystemDialogMock::ShowMsgDialog(const std::string& /*message*/, DialogButtonResult auto_response) {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_status == DialogStatus::Running) return false;

	m_active_type   = DialogType::MsgDialog;
	m_status        = DialogStatus::Running;
	m_button_result = auto_response;
	m_stats.total_dialogs_opened++;

	return true;
}

bool Ps5SystemDialogMock::ShowImeDialog(const std::string& /*title*/, const std::string& /*initial_text*/, const std::string& mock_input_text) {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_status == DialogStatus::Running) return false;

	m_active_type   = DialogType::ImeDialog;
	m_status        = DialogStatus::Running;
	m_ime_result    = mock_input_text;
	m_button_result = DialogButtonResult::Ok;
	m_stats.total_dialogs_opened++;

	return true;
}

void Ps5SystemDialogMock::Update() {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_status == DialogStatus::Running) {
		m_status = DialogStatus::Finished; // Transition automatically to finished
	}
}

void Ps5SystemDialogMock::CloseDialog() {
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_status != DialogStatus::None) {
		m_active_type = DialogType::None;
		m_status      = DialogStatus::None;
		m_stats.total_dialogs_closed++;
	}
}

void Ps5SystemDialogMock::Reset() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_active_type   = DialogType::None;
	m_status        = DialogStatus::None;
	m_button_result = DialogButtonResult::Ok;
	m_ime_result.clear();
	m_stats = {};
}

} // namespace Kernel
