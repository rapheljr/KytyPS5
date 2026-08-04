// macOsHidBackend.h
//
// Apple macOS IOKit HID Hardware Backend for DualSense Controller.

#ifndef INPUT_MACOS_HID_BACKEND_H
#define INPUT_MACOS_HID_BACKEND_H

#include "common/common.h"
#include "input/inputEngine.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <unordered_map>

#if defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#endif

namespace Libs::Input {

constexpr uint32_t kSonyVendorId               = 0x054C;
constexpr uint32_t kDualSenseProductId         = 0x0CE6;
constexpr uint32_t kDualSenseEdgeProductId     = 0x0DF2;

class MacOsHidBackend : public IInputBackend {
public:
	MacOsHidBackend();
	~MacOsHidBackend() override;

	KYTY_CLASS_NO_COPY(MacOsHidBackend);

	bool Initialize() override;
	void Shutdown() override;
	bool PollInput(uint32_t controller_index, DualSenseState& out_state) override;
	bool SendOutputReport(uint32_t controller_index, const DualSenseState& state) override;
	const char* GetBackendName() const override { return "macOS IOKit DualSense HID"; }

	static bool DecodeDualSenseInputReport(const uint8_t* report, size_t size, DualSenseState& out_state);
	static size_t EncodeDualSenseOutputReport(const DualSenseState& state, uint8_t* out_report, size_t max_size);

private:
	std::atomic<bool>  m_initialized{false};
	mutable std::mutex m_mutex;

#if defined(__APPLE__)
	IOHIDManagerRef    m_hid_manager = nullptr;
#endif
};

} // namespace Libs::Input

#endif // INPUT_MACOS_HID_BACKEND_H
