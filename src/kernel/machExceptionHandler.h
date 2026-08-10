// machExceptionHandler.h
//
// Apple Silicon Native Darwin Mach Exception Server & Fault Recovery Engine.
// Intercepts hardware memory access faults (EXC_BAD_ACCESS) and invalid instructions (EXC_BAD_INSTRUCTION)
// via dedicated Mach exception ports on ARM64 host threads.

#ifndef KERNEL_MACH_EXCEPTION_HANDLER_H
#define KERNEL_MACH_EXCEPTION_HANDLER_H

#include "common/common.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/exception_types.h>
#endif

namespace Kernel {

enum class MachExceptionType {
	BadAccess,
	BadInstruction,
	Arithmetic,
	Other
};

struct MachExceptionContext {
	MachExceptionType type = MachExceptionType::Other;
	uint64_t          fault_address = 0;
	uint64_t          pc = 0;
	uint64_t          lr = 0;
	uint64_t          sp = 0;
	uint64_t          thread_id = 0;
	bool              is_write = false;
};

using MachFaultCallback = std::function<bool(const MachExceptionContext& ctx)>;

class MachExceptionHandler {
public:
	MachExceptionHandler();
	~MachExceptionHandler();

	KYTY_CLASS_NO_COPY(MachExceptionHandler);

	bool Initialize();
	void Shutdown();

	bool InstallThreadHandler(uint64_t mach_thread_port);
	bool RemoveThreadHandler(uint64_t mach_thread_port);

	void SetFaultCallback(MachFaultCallback callback) { m_callback = std::move(callback); }
	[[nodiscard]] bool IsActive() const noexcept { return m_active.load(); }
	[[nodiscard]] uint64_t GetHandledExceptionsCount() const noexcept { return m_handled_count.load(); }

private:
	void ServerThreadMain();

	std::atomic<bool>     m_active{false};
	std::atomic<uint64_t> m_handled_count{0};
	MachFaultCallback     m_callback;
	std::thread           m_server_thread;

#if defined(__APPLE__)
	mach_port_t           m_exception_port = MACH_PORT_NULL;
#endif
};

} // namespace Kernel

#endif // KERNEL_MACH_EXCEPTION_HANDLER_H
