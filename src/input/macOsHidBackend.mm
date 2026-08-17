// macOsHidBackend.mm
//
// Apple macOS IOKit HID Hardware Backend Implementation for DualSense Controller.

#include "input/macOsHidBackend.h"

#include <cmath>
#include <cstring>

namespace Libs::Input {

bool MacOsHidBackend::DecodeDualSenseInputReport(const uint8_t* report, size_t size, DualSenseState& out_state) {
	if (!report || size < 10) return false;

	size_t offset = (size >= 78) ? 2 : 0; // Skip Bluetooth header if 78-byte BT report

	out_state.left_stick_x  = report[offset + 1];
	out_state.left_stick_y  = report[offset + 2];
	out_state.right_stick_x = report[offset + 3];
	out_state.right_stick_y = report[offset + 4];

	out_state.trigger_l2    = report[offset + 5];
	out_state.trigger_r2    = report[offset + 6];

	uint8_t dpad_buttons = report[offset + 8];
	uint8_t buttons2      = report[offset + 9];
	uint8_t buttons3      = report[offset + 10];

	uint32_t btn_mask = 0;

	// D-Pad decoding (bits 0..3)
	uint8_t dpad = dpad_buttons & 0x0F;
	switch (dpad) {
		case 0: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadUp); break;
		case 1: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadUp) | static_cast<uint32_t>(DualSenseButtons::DPadRight); break;
		case 2: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadRight); break;
		case 3: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadDown) | static_cast<uint32_t>(DualSenseButtons::DPadRight); break;
		case 4: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadDown); break;
		case 5: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadDown) | static_cast<uint32_t>(DualSenseButtons::DPadLeft); break;
		case 6: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadLeft); break;
		case 7: btn_mask |= static_cast<uint32_t>(DualSenseButtons::DPadUp) | static_cast<uint32_t>(DualSenseButtons::DPadLeft); break;
		default: break;
	}

	// Action buttons (bits 4..7 of byte 8)
	if (dpad_buttons & 0x10) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Square);
	if (dpad_buttons & 0x20) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Cross);
	if (dpad_buttons & 0x40) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Circle);
	if (dpad_buttons & 0x80) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Triangle);

	// Shoulder & System buttons (byte 9)
	if (buttons2 & 0x01) btn_mask |= static_cast<uint32_t>(DualSenseButtons::L1);
	if (buttons2 & 0x02) btn_mask |= static_cast<uint32_t>(DualSenseButtons::R1);
	if (buttons2 & 0x04) btn_mask |= static_cast<uint32_t>(DualSenseButtons::L2);
	if (buttons2 & 0x08) btn_mask |= static_cast<uint32_t>(DualSenseButtons::R2);
	if (buttons2 & 0x10) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Create);
	if (buttons2 & 0x20) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Options);
	if (buttons2 & 0x40) btn_mask |= static_cast<uint32_t>(DualSenseButtons::L3);
	if (buttons2 & 0x80) btn_mask |= static_cast<uint32_t>(DualSenseButtons::R3);

	// PS, TouchPad & Mute (byte 10)
	if (buttons3 & 0x01) btn_mask |= static_cast<uint32_t>(DualSenseButtons::PS);
	if (buttons3 & 0x02) btn_mask |= static_cast<uint32_t>(DualSenseButtons::TouchClick);
	if (buttons3 & 0x04) btn_mask |= static_cast<uint32_t>(DualSenseButtons::Mute);

	out_state.buttons = static_cast<DualSenseButtons>(btn_mask);

	// IMU decoding (if report is long enough)
	if (size >= offset + 28) {
		int16_t gx = 0, gy = 0, gz = 0;
		int16_t ax = 0, ay = 0, az = 0;
		std::memcpy(&gx, report + offset + 16, 2);
		std::memcpy(&gy, report + offset + 18, 2);
		std::memcpy(&gz, report + offset + 20, 2);
		std::memcpy(&ax, report + offset + 22, 2);
		std::memcpy(&ay, report + offset + 24, 2);
		std::memcpy(&az, report + offset + 26, 2);

		out_state.imu.gyro_x  = static_cast<float>(gx) / 1024.0f;
		out_state.imu.gyro_y  = static_cast<float>(gy) / 1024.0f;
		out_state.imu.gyro_z  = static_cast<float>(gz) / 1024.0f;
		out_state.imu.accel_x = static_cast<float>(ax) / 8192.0f;
		out_state.imu.accel_y = static_cast<float>(ay) / 8192.0f;
		out_state.imu.accel_z = static_cast<float>(az) / 8192.0f;
	}

	// Touchpad decoding (if report is long enough)
	if (size >= offset + 40) {
		uint8_t t0_id = report[offset + 33];
		out_state.touch0.active = (t0_id & 0x80) == 0;
		out_state.touch0.id     = t0_id & 0x7F;
		out_state.touch0.x      = static_cast<uint16_t>(report[offset + 34]) | (static_cast<uint16_t>(report[offset + 35] & 0x0F) << 8);
		out_state.touch0.y      = (static_cast<uint16_t>(report[offset + 35] & 0xF0) >> 4) | (static_cast<uint16_t>(report[offset + 36]) << 4);

		uint8_t t1_id = report[offset + 37];
		out_state.touch1.active = (t1_id & 0x80) == 0;
		out_state.touch1.id     = t1_id & 0x7F;
		out_state.touch1.x      = static_cast<uint16_t>(report[offset + 38]) | (static_cast<uint16_t>(report[offset + 39] & 0x0F) << 8);
		out_state.touch1.y      = (static_cast<uint16_t>(report[offset + 39] & 0xF0) >> 4) | (static_cast<uint16_t>(report[offset + 40]) << 4);
	}

	out_state.connected = true;
	return true;
}

size_t MacOsHidBackend::EncodeDualSenseOutputReport(const DualSenseState& state, uint8_t* out_report, size_t max_size) {
	if (!out_report || max_size < 48) return 0;

	std::memset(out_report, 0, max_size);

	out_report[0] = 0x02; // DualSense Output Report ID
	out_report[1] = 0xFF; // Enable flags (Rumble, LEDs, Triggers)
	out_report[2] = 0xF7;

	// Motor Rumble
	out_report[3] = state.haptics.small_motor_rumble;
	out_report[4] = state.haptics.large_motor_rumble;

	// Lightbar RGB
	out_report[44] = state.lightbar.r;
	out_report[45] = state.lightbar.g;
	out_report[46] = state.lightbar.b;

	// Adaptive Trigger L2 effect
	out_report[11] = static_cast<uint8_t>(state.trigger_l2_effect.mode);
	out_report[12] = state.trigger_l2_effect.start_position;
	out_report[13] = state.trigger_l2_effect.force_strength;
	out_report[14] = state.trigger_l2_effect.frequency;

	// Adaptive Trigger R2 effect
	out_report[22] = static_cast<uint8_t>(state.trigger_r2_effect.mode);
	out_report[23] = state.trigger_r2_effect.start_position;
	out_report[24] = state.trigger_r2_effect.force_strength;
	out_report[25] = state.trigger_r2_effect.frequency;

	return 48;
}

MacOsHidBackend::MacOsHidBackend() = default;

MacOsHidBackend::~MacOsHidBackend() {
	Shutdown();
}

bool MacOsHidBackend::Initialize() {
#if defined(__APPLE__)
	std::lock_guard<std::mutex> lock(m_mutex);
	m_hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!m_hid_manager) return false;

	IOHIDManagerSetDeviceMatching(m_hid_manager, nullptr);
	IOHIDManagerOpen(m_hid_manager, kIOHIDOptionsTypeNone);
	m_initialized = true;
	return true;
#else
	m_initialized = true;
	return true;
#endif
}

void MacOsHidBackend::Shutdown() {
#if defined(__APPLE__)
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_hid_manager) {
		IOHIDManagerClose(m_hid_manager, kIOHIDOptionsTypeNone);
		CFRelease(m_hid_manager);
		m_hid_manager = nullptr;
	}
#endif
	m_initialized = false;
}

bool MacOsHidBackend::PollInput(uint32_t /*controller_index*/, DualSenseState& out_state) {
	if (!m_initialized) return false;
	out_state.connected = true;
	return true;
}

bool MacOsHidBackend::SendOutputReport(uint32_t /*controller_index*/, const DualSenseState& state) {
	if (!m_initialized) return false;

	uint8_t report_buf[64] = {0};
	size_t report_len = EncodeDualSenseOutputReport(state, report_buf, sizeof(report_buf));
	if (report_len == 0) return false;

#if defined(__APPLE__)
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_hid_manager) {
		CFSetRef device_set = IOHIDManagerCopyDevices(m_hid_manager);
		if (device_set) {
			CFIndex count = CFSetGetCount(device_set);
			if (count > 0) {
				std::vector<const void*> devices(count);
				CFSetGetValues(device_set, devices.data());
				for (CFIndex i = 0; i < count; ++i) {
					auto dev = static_cast<IOHIDDeviceRef>(const_cast<void*>(devices[i]));
					IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, report_buf[0], report_buf, report_len);
				}
			}
			CFRelease(device_set);
		}
	}
#endif
	return true;
}

} // namespace Libs::Input
