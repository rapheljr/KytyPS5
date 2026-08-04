// ps5Vfs.cpp
//
// Virtual Filesystem & Package Mounting for Phase N PS5 Kernel Emulation.

#include "kernel/ps5Vfs.h"

#include <sys/stat.h>

#include <cstdio>

namespace Libs::Kernel::Ps5 {

bool VirtualFileSystem::Mount(const std::string& guest_path, const std::string& host_path) {
	if (guest_path.empty() || host_path.empty()) return false;
	m_mounts[guest_path] = host_path;
	return true;
}

bool VirtualFileSystem::Unmount(const std::string& guest_path) {
	return m_mounts.erase(guest_path) > 0;
}

std::string VirtualFileSystem::ResolvePath(const std::string& guest_path) const {
	for (const auto& [mount_guest, mount_host] : m_mounts) {
		if (guest_path.rfind(mount_guest, 0) == 0) {
			std::string sub = guest_path.substr(mount_guest.length());
			if (!sub.empty() && sub[0] == '/') sub = sub.substr(1);
			return mount_host + "/" + sub;
		}
	}
	return guest_path;
}

int32_t VirtualFileSystem::OpenFile(const std::string& guest_path, VfsOpenFlags) {
	std::string resolved = ResolvePath(guest_path);
	int32_t fd = m_next_fd++;
	m_open_files[fd] = resolved;
	return fd;
}

bool VirtualFileSystem::CloseFile(int32_t fd) {
	return m_open_files.erase(fd) > 0;
}

int64_t VirtualFileSystem::ReadFile(int32_t fd, void*, size_t) {
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;
	return 0; // Simulated read
}

int64_t VirtualFileSystem::WriteFile(int32_t fd, const void*, size_t size) {
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;
	return static_cast<int64_t>(size);
}

bool VirtualFileSystem::StatPath(const std::string& guest_path, VfsStat& out_stat) const {
	std::string resolved = ResolvePath(guest_path);
	struct stat st{};
	if (::stat(resolved.c_str(), &st) == 0) {
		out_stat.size_bytes = static_cast<size_t>(st.st_size);
		out_stat.is_directory = S_ISDIR(st.st_mode);
		out_stat.modified_time = static_cast<uint64_t>(st.st_mtime);
		return true;
	}
	return false;
}

} // namespace Libs::Kernel::Ps5
