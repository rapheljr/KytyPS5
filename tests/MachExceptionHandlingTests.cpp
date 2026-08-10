// MachExceptionHandlingTests.cpp
//
// Unit Tests for Apple Silicon Native Darwin Mach Exception Server.

#include "kernel/machExceptionHandler.h"

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <atomic>

namespace {

void Check(bool cond, const char* msg) {
	if (!cond) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Kernel;

void TestMachHandlerLifecycle() {
	std::printf("[TEST] MachHandler_Lifecycle\n");

	MachExceptionHandler handler;
	Check(!handler.IsActive(), "Handler should not be active before Initialize()");

	bool init_ok = handler.Initialize();
	Check(init_ok, "MachExceptionHandler Initialize failed");
	Check(handler.IsActive(), "Handler should be active after Initialize()");

	handler.SetFaultCallback([](const MachExceptionContext& ctx) -> bool {
		std::printf("  [MachCallback] Intercepted exception: type=%d, fault_addr=0x%llx\n",
		            static_cast<int>(ctx.type), (unsigned long long)ctx.fault_address);
		return true;
	});

	bool install_ok = handler.InstallThreadHandler(0);
	Check(install_ok, "InstallThreadHandler on current thread failed");

	bool remove_ok = handler.RemoveThreadHandler(0);
	Check(remove_ok, "RemoveThreadHandler on current thread failed");

	handler.Shutdown();
	Check(!handler.IsActive(), "Handler should not be active after Shutdown()");

	std::printf("  [ OK ] MachHandler_Lifecycle\n");
}

void TestMachHandlerMultipleInitShutdown() {
	std::printf("[TEST] MachHandler_MultipleInitShutdown\n");

	MachExceptionHandler handler;
	for (int i = 0; i < 3; ++i) {
		bool ok = handler.Initialize();
		Check(ok, "Repeated Initialize failed");
		Check(handler.IsActive(), "Handler should be active");
		handler.Shutdown();
		Check(!handler.IsActive(), "Handler should be inactive");
	}

	std::printf("  [ OK ] MachHandler_MultipleInitShutdown\n");
}

} // namespace

int main() {
	std::setbuf(stdout, nullptr);
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Apple Silicon Native Darwin Mach Exception Test Suite\n");
	std::printf("================================================================================\n");

	TestMachHandlerLifecycle();
	TestMachHandlerMultipleInitShutdown();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");
	return 0;
}
