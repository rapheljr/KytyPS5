// inputEngine.h
//
// Platform-Independent Input Engine & Interface for DualSense Controller Emulation.

#ifndef INPUT_INPUT_ENGINE_H
#define INPUT_INPUT_ENGINE_H

#include "common/common.h"
#include "input/dualSenseState.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace Libs::Input {

constexpr uint32_t kMaxControllers = 4;

class IInputBackend {
public:
	virtual ~IInputBackend() = default;

	virtual bool Initialize() = 0;
	virtual void Shutdown() = 0;
	virtual bool PollInput(uint32_t controller_index, DualSenseState& out_state) = 0;
	virtual bool SendOutputReport(uint32_t controller_index, const DualSenseState& state) = 0;
	virtual const char* GetBackendName() const = 0;
};

class DualSenseController {
public:
	DualSenseController(uint32_t index = 0);
	~DualSenseController() = default;

	KYTY_CLASS_NO_COPY(DualSenseController);

	void SetState(const DualSenseState& state);
	[[nodiscard]] DualSenseState GetState() const;

	void SetButton(DualSenseButtons button, bool pressed);
	void SetAnalogSticks(uint8_t lx, uint8_t ly, uint8_t rx, uint8_t ry);
	void SetTriggers(uint8_t l2, uint8_t r2);
	void SetAdaptiveTrigger(bool is_r2, const AdaptiveTriggerConfig& config);

	void SetTouch0(bool active, uint16_t x, uint16_t y, uint8_t id = 0);
	void SetTouch1(bool active, uint16_t x, uint16_t y, uint8_t id = 1);
	void SetIMU(float gyro_x, float gyro_y, float gyro_z, float accel_x, float accel_y, float accel_z);

	void SetVibration(uint8_t small_motor, uint8_t large_motor);
	void SetLightbar(uint8_t r, uint8_t g, uint8_t b);

	[[nodiscard]] uint32_t GetIndex() const noexcept { return m_index; }

private:
	uint32_t           m_index = 0;
	DualSenseState     m_state;
	mutable std::mutex m_mutex;
};

class InputManager {
public:
	InputManager();
	~InputManager();

	KYTY_CLASS_NO_COPY(InputManager);

	bool Initialize(std::unique_ptr<IInputBackend> backend = nullptr);
	void Shutdown();

	void PollAll();

	[[nodiscard]] DualSenseController* GetController(uint32_t index);
	[[nodiscard]] IInputBackend* GetBackend() noexcept { return m_backend.get(); }

private:
	std::unique_ptr<IInputBackend>                                     m_backend;
	std::array<std::unique_ptr<DualSenseController>, kMaxControllers> m_controllers;
	mutable std::mutex                                                 m_mutex;
};

} // namespace Libs::Input

#endif // INPUT_INPUT_ENGINE_H
