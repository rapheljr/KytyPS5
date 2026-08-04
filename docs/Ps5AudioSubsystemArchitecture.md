# PS5 Audio Subsystem Architecture

## Overview

The KytyPS5 Audio Subsystem provides a backend-independent, low-latency, multi-channel spatial audio pipeline for emulating PS5 `AudioOut` and `AudioOut2` libKernel services.

---

## Architecture Diagram

```
+-------------------------------------------------------------------+
|                    Guest Application (libAudio / AudioOut2)       |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                        AudioEngine                                |
|   +-----------------------------------------------------------+   |
|   | VoiceMixer (Up to 64 active voice channels)                |   |
|   | - Atomic PCM ring-buffers                                 |   |
|   | - Gain, volume scaling, float32 clipping                  |   |
|   +-----------------------------------------------------------+   |
|   | Audio3DEngine                                             |   |
|   | - VBAP (Vector Base Amplitude Panning) for 7.1.4          |   |
|   | - Ambisonics 1st-order spherical harmonics                |   |
|   | - Height-aware panning & distance attenuation             |   |
|   +-----------------------------------------------------------+   |
+-------------------------------------------------------------------+
                                  |
                                  v
+-------------------------------------------------------------------+
|                     IAudioBackend Interface                       |
|   +-----------------------------------------------------------+   |
|   | CoreAudioBackend (Apple macOS AudioUnit HAL Output)       |   |
|   | Sub-10ms latency render callback (AURenderCallback)       |   |
|   +-----------------------------------------------------------+   |
|   | Mock / Fallback Backend (Headless & Cross-Platform)       |   |
|   +-----------------------------------------------------------+   |
+-------------------------------------------------------------------+
```

---

## Features

1. **Backend-Independent Abstraction (`IAudioBackend`)**:
   - Decouples host audio drivers from guest audio API logic.
   - CoreAudio HAL hardware backend for macOS (`AudioComponentInstance`, `kAudioUnitType_Output`).

2. **Multi-Voice Mixer (`VoiceMixer`)**:
   - Up to 64 concurrent active voice streams (`AudioVoice`).
   - Float32 master buffer rendering with per-voice volume scaling, 12-channel panning matrix, and pitch adjustment.

3. **3D Spatial Audio (`Audio3DEngine`)**:
   - Vector Base Amplitude Panning (VBAP) for 7.1.4 surround setups (L, R, C, LFE, Ls, Rs, Lb, Rb, Tfl, Tfr, Tbl, Tbr).
   - Distance-based attenuation curves, height-aware panning, and 3D latency configuration.

4. **Synchronization & Latency Control**:
   - Target latency control (5 ms to 50 ms).
   - Real-time audio hardware time tracking (`GetAudioClockTimeUs`) for synchronization with `FrameScheduler`.
