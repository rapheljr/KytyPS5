# PS5 DualSense Input Architecture

## Overview

The KytyPS5 Input Subsystem provides complete DualSense wireless controller emulation and a platform-independent input backend architecture supporting buttons, analog sticks, analog triggers, adaptive trigger force feedback modes, rumble motors, haptic audio waveforms, dual-point multi-touch touchpad tracking, 6-DOF IMU motion sensors (gyroscope and accelerometer), controller speaker output, and controller microphone input.

---

## Architecture Diagram

```
+-------------------------------------------------------------------+
|                        Guest Application                          |
|                  (libPad / controller syscalls)                   |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                           InputManager                            |
|  +-------------------------------------------------------------+  |
|  | DualSenseController [0..3] (Track complete controller state) |  |
|  | - Buttons: Cross, Circle, Square, Triangle, DPad, L1..R3... |  |
|  | - Analog Sticks: LX, LY, RX, RY (0..255)                    |  |
|  | - Adaptive Triggers: L2/R2 (Off, Weapon, Vibration, Slope)  |  |
|  | - Haptics: Dual motors + PCM Waveform                       |  |
|  | - Touchpad: Dual-point touch tracking (1920x1080)          |  |
|  | - IMU 6-DOF: Gyroscope (X/Y/Z) & Accelerometer (X/Y/Z)      |  |
|  +-------------------------------------------------------------+  |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                      IInputBackend Interface                      |
|  +-------------------------------------------------------------+  |
|  | MacOsHidBackend (Native Apple IOKit IOHIDManager Driver)    |  |
|  | Vendor ID: 0x054C (Sony), Product ID: 0x0CE6 / 0x0DF2         |  |
|  | USB (64-byte) & Bluetooth (78-byte) Input/Output HID Reports  |  |
|  +-------------------------------------------------------------+  |
|  | Mock / Synthetic Input Backend (Headless & Testing)          |  |
|  +-------------------------------------------------------------+  |
+-------------------------------------------------------------------+
```

---

## Supported Input Features

| Feature | Data Representation | Range / Resolution |
|---------|---------------------|--------------------|
| Buttons | `DualSenseButtons` bitmask | 19 digital buttons |
| Analog Sticks | `left_stick_x/y`, `right_stick_x/y` | 8-bit unsigned integer (0..255, 128 center) |
| Triggers | `trigger_l2`, `trigger_r2` | 8-bit analog (0..255) |
| Adaptive Triggers | `AdaptiveTriggerConfig` | Modes: `Off`, `Feedback`, `Weapon`, `Vibration`, `SlopeFeedback`, `MultiplePosition` |
| Vibration | `small_motor_rumble`, `large_motor_rumble` | 8-bit pulse width modulation (0..255) |
| Haptics | `haptic_left/right_amplitude`, PCM waveform | Audio-rate float32 PCM feedback buffer |
| Touchpad | `touch0`, `touch1` | Active, Touch ID, X: 0..1920, Y: 0..1080 |
| Gyroscope | `imu.gyro_x/y/z` | Angular velocity (rad/s or deg/s) |
| Accelerometer | `imu.accel_x/y/z` | Acceleration g-force (1.0g gravity baseline) |
| Speaker & Mic | `speaker_pcm_out`, `mic_pcm_in` | Controller audio stream buffers |

---

## macOS IOKit DualSense HID Protocol

The `MacOsHidBackend` communicates directly with Sony DualSense controllers over USB and Bluetooth using `IOHIDManager`:
- **USB Input Report**: 64 bytes.
- **Bluetooth Input Report**: 78 bytes (with 2-byte BT header prefix).
- **Output Report**: 48 bytes (USB) / 74 bytes (Bluetooth) specifying rumble intensities, lightbar RGB colors, and adaptive trigger force parameters.
