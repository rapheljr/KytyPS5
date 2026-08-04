// ps5Network.h
//
// Timers, Sockets & Async Network Event Queues for Phase N PS5 Kernel Emulation.

#ifndef KERNEL_PS5_NETWORK_H
#define KERNEL_PS5_NETWORK_H

#include "common/common.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace Libs::Kernel::Ps5 {

struct KernelTimeSpec {
	int64_t sec  = 0;
	int64_t nsec = 0;
};

class KernelClock {
public:
	static KernelTimeSpec GetTimeNanoseconds() noexcept;
	static void SleepMicroseconds(uint64_t usec);
};

enum class SocketType : uint8_t {
	Stream = 1, // TCP
	Dgram  = 2  // UDP
};

class NetworkManager {
public:
	NetworkManager() = default;
	~NetworkManager() = default;

	KYTY_CLASS_NO_COPY(NetworkManager);

	int32_t CreateSocket(SocketType type);
	bool Bind(int32_t sock_fd, const std::string& ip_addr, uint16_t port);
	bool Listen(int32_t sock_fd, int backlog);
	bool CloseSocket(int32_t sock_fd);

	[[nodiscard]] size_t GetActiveSocketCount() const noexcept { return m_sockets.size(); }

private:
	int32_t m_next_sock_fd = 100;
	std::unordered_map<int32_t, SocketType> m_sockets;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_NETWORK_H
