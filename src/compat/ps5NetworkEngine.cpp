// ps5NetworkEngine.cpp
//
// PS5 Virtual Network Socket & SceNetCtl Engine Implementation.

#include "compat/ps5NetworkEngine.h"
#include "common/logging/log.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

namespace Compat {

Ps5NetworkEngine::Ps5NetworkEngine() = default;

Ps5NetworkEngine::~Ps5NetworkEngine() {
	Shutdown();
}

bool Ps5NetworkEngine::Initialize() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_initialized) {
		return true;
	}

	m_ctl_info = SceNetCtlInfo{};
	m_socket_table.clear();
	m_next_handle = 1000;
	m_initialized = true;
	return true;
}

void Ps5NetworkEngine::Shutdown() {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized) {
		return;
	}

	for (auto& [handle, host_fd] : m_socket_table) {
		if (host_fd >= 0) {
			::close(host_fd);
		}
	}
	m_socket_table.clear();
	m_initialized = false;
}

int32_t Ps5NetworkEngine::Socket(int32_t domain, int32_t type, int32_t protocol) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized) {
		return -1;
	}

	int host_fd = ::socket(domain, type, protocol);
	if (host_fd < 0) {
		return -1;
	}

	int32_t handle = m_next_handle++;
	m_socket_table[handle] = host_fd;
	return handle;
}

int32_t Ps5NetworkEngine::Bind(int32_t s, const SceNetSockAddrIn* addr, uint32_t addrlen) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized || !addr) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	sockaddr_in host_addr{};
	host_addr.sin_len = sizeof(sockaddr_in);
	host_addr.sin_family = addr->sin_family;
	host_addr.sin_port = addr->sin_port;
	host_addr.sin_addr.s_addr = addr->sin_addr;

	return ::bind(it->second, reinterpret_cast<const sockaddr*>(&host_addr), sizeof(host_addr));
}

int32_t Ps5NetworkEngine::Listen(int32_t s, int32_t backlog) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	return ::listen(it->second, backlog);
}

int32_t Ps5NetworkEngine::Accept(int32_t s, SceNetSockAddrIn* addr, uint32_t* addrlen) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	sockaddr_in host_addr{};
	socklen_t len = sizeof(host_addr);
	int client_fd = ::accept(it->second, reinterpret_cast<sockaddr*>(&host_addr), &len);
	if (client_fd < 0) {
		return -1;
	}

	if (addr) {
		addr->sin_family = host_addr.sin_family;
		addr->sin_port = host_addr.sin_port;
		addr->sin_addr = host_addr.sin_addr.s_addr;
	}
	if (addrlen) {
		*addrlen = sizeof(SceNetSockAddrIn);
	}

	int32_t client_handle = m_next_handle++;
	m_socket_table[client_handle] = client_fd;
	return client_handle;
}

int32_t Ps5NetworkEngine::Connect(int32_t s, const SceNetSockAddrIn* addr, uint32_t addrlen) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized || !addr) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	sockaddr_in host_addr{};
	host_addr.sin_len = sizeof(sockaddr_in);
	host_addr.sin_family = addr->sin_family;
	host_addr.sin_port = addr->sin_port;
	host_addr.sin_addr.s_addr = addr->sin_addr;

	return ::connect(it->second, reinterpret_cast<const sockaddr*>(&host_addr), sizeof(host_addr));
}

int32_t Ps5NetworkEngine::Send(int32_t s, const void* msg, size_t len, int32_t flags) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized || !msg) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	return static_cast<int32_t>(::send(it->second, msg, len, flags));
}

int32_t Ps5NetworkEngine::Recv(int32_t s, void* buf, size_t len, int32_t flags) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized || !buf) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	return static_cast<int32_t>(::recv(it->second, buf, len, flags));
}

int32_t Ps5NetworkEngine::Close(int32_t s) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized) {
		return -1;
	}

	auto it = m_socket_table.find(s);
	if (it == m_socket_table.end()) {
		return -1;
	}

	::close(it->second);
	m_socket_table.erase(it);
	return 0;
}

bool Ps5NetworkEngine::GetCtlInfo(SceNetCtlInfo* out_info) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_initialized || !out_info) {
		return false;
	}
	*out_info = m_ctl_info;
	return true;
}

void Ps5NetworkEngine::SetCtlState(SceNetCtlState state) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_ctl_info.state = state;
}

uint32_t Ps5NetworkEngine::GetActiveSocketCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return static_cast<uint32_t>(m_socket_table.size());
}

} // namespace Compat
