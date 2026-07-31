#include "common/hostException.h"
#include "common/config.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#if KYTY_PLATFORM != KYTY_PLATFORM_WINDOWS
#include <pthread.h>
#include <csignal>
#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
#include <mach/mach.h>
#endif
#endif

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "SignalHandlingTests: failed: %s\n", text);
		std::abort();
	}
}

bool TestHandler(const Common::HostException::ExceptionInfo& info) {
	Check(info.type == Common::HostException::ExceptionType::AccessViolation ||
	          info.type == Common::HostException::ExceptionType::IllegalInstruction,
	      "exception type must be valid");
	return false;
}

void TestInstallHandler() {
	Check(Common::HostException::InstallHandler(TestHandler),
	      "InstallHandler should succeed");
	Check(Common::HostException::InstallHandler(TestHandler),
	      "Re-installing same or new handler should succeed");
}

void TestSignalUcontextStructLayout() {
	Check(sizeof(uint64_t) == 8, "uint64_t must be 8 bytes");
#if defined(__APPLE__) || KYTY_PLATFORM == KYTY_PLATFORM_MACOS
	mach_port_t port1 = pthread_mach_thread_np(pthread_self());
	mach_port_t port2 = pthread_mach_thread_np(pthread_self());
	Check(port1 != 0, "pthread_mach_thread_np must return non-zero port");
	Check(port1 == port2, "pthread_mach_thread_np must be consistent for same thread");
#endif
}

} // namespace

int main() {
	TestInstallHandler();
	TestSignalUcontextStructLayout();

	std::printf("SignalHandlingTests: PASSED\n");
	return 0;
}
