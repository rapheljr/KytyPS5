#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace Compat {

enum class SceNetSocketType {
	Stream = 1, // SOCK_STREAM (TCP)
	Dgram = 2,  // SOCK_DGRAM (UDP)
	Raw = 3     // SOCK_RAW
};

enum class SceNetCtlState {
	Disconnected = 0,
	Connecting = 1,
	Connected = 2,
	IpObtaining = 3,
	IpObtained = 4
};

struct SceNetCtlInfo {
	SceNetCtlState state = SceNetCtlState::IpObtained;
	std::string ip_address = "192.168.1.105";
	std::string netmask = "255.255.255.0";
	std::string gateway = "192.168.1.1";
	std::string primary_dns = "8.8.8.8";
	std::string secondary_dns = "8.8.4.4";
	std::string mac_address = "00:D9:D1:4F:2E:8C";
	uint32_t mtu = 1500;
	uint32_t link_speed_mbps = 1000;
};

struct SceNetSockAddrIn {
	uint8_t sin_len = sizeof(SceNetSockAddrIn);
	uint8_t sin_family = 2; // AF_INET
	uint16_t sin_port = 0;
	uint32_t sin_addr = 0;
	char sin_zero[8] = {0};
};

class Ps5NetworkEngine {
public:
	Ps5NetworkEngine();
	~Ps5NetworkEngine();

	bool Initialize();
	void Shutdown();

	bool IsInitialized() const { return m_initialized; }

	// SceNet Socket API Wrappers
	int32_t Socket(int32_t domain, int32_t type, int32_t protocol);
	int32_t Bind(int32_t s, const SceNetSockAddrIn* addr, uint32_t addrlen);
	int32_t Listen(int32_t s, int32_t backlog);
	int32_t Accept(int32_t s, SceNetSockAddrIn* addr, uint32_t* addrlen);
	int32_t Connect(int32_t s, const SceNetSockAddrIn* addr, uint32_t addrlen);
	int32_t Send(int32_t s, const void* msg, size_t len, int32_t flags);
	int32_t Recv(int32_t s, void* buf, size_t len, int32_t flags);
	int32_t Close(int32_t s);

	// SceNetCtl Status API
	bool GetCtlInfo(SceNetCtlInfo* out_info) const;
	void SetCtlState(SceNetCtlState state);

	uint32_t GetActiveSocketCount() const;

private:
	bool m_initialized = false;
	SceNetCtlInfo m_ctl_info;
	mutable std::mutex m_mutex;
	int32_t m_next_handle = 1000;
	std::unordered_map<int32_t, int> m_socket_table; // Maps guest handle to host socket fd
};

} // namespace Compat
