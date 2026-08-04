// ps5Network.cpp
//
// Timers, Sockets & Async Network Event Queues for Phase N PS5 Kernel Emulation.

#include "kernel/ps5Network.h"

#include <thread>

namespace Libs::Kernel::Ps5 {

KernelTimeSpec KernelClock::GetTimeNanoseconds() noexcept {
	auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
	auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

	KernelTimeSpec ts{};
	ts.sec  = ns / 1000000000LL;
	ts.nsec = ns % 1000000000LL;
	return ts;
}

void KernelClock::SleepMicroseconds(uint64_t usec) {
	std::this_thread::sleep_for(std::chrono::microseconds(usec));
}

// ─── NetworkManager ──────────────────────────────────────────────────────────

int32_t NetworkManager::CreateSocket(SocketType type) {
	int32_t fd = m_next_sock_fd++;
	m_sockets[fd] = type;
	return fd;
}

bool NetworkManager::Bind(int32_t sock_fd, const std::string&, uint16_t) {
	return m_sockets.find(sock_fd) != m_sockets.end();
}

bool NetworkManager::Listen(int32_t sock_fd, int) {
	return m_sockets.find(sock_fd) != m_sockets.end();
}

bool NetworkManager::CloseSocket(int32_t sock_fd) {
	return m_sockets.erase(sock_fd) > 0;
}

} // namespace Libs::Kernel::Ps5
