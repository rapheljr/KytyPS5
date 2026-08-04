// ps5Vfs.cpp
//
// Unified PS5 Virtual Filesystem, Package Mount & Path Translation Engine Implementation.

#include "kernel/ps5Vfs.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace Libs::Kernel::Ps5 {

VirtualFileSystem::VirtualFileSystem() = default;

bool VirtualFileSystem::Mount(const std::string& guest_path, const std::string& host_path, MountType type, MountFlags flags, uint32_t priority) {
	return m_mount_manager.Mount(guest_path, host_path, type, flags, priority);
}

bool VirtualFileSystem::Unmount(const std::string& guest_path) {
	return m_mount_manager.Unmount(guest_path);
}

void VirtualFileSystem::MountStandardPs5Directories(const std::string& base_host_dir) {
	m_mount_manager.MountStandardPs5Directories(base_host_dir);
}

bool VirtualFileSystem::InstallPackageBuffer(const uint8_t* pkg_data, size_t size, const std::string& target_dir, std::string* out_content_id) {
	return PkgInstaller::InstallPackageBuffer(pkg_data, size, target_dir, out_content_id);
}

std::string VirtualFileSystem::ResolvePath(const std::string& guest_path) const {
	return PathTranslator::TranslateToHostPath(guest_path, m_mount_manager);
}

int32_t VirtualFileSystem::OpenFile(const std::string& guest_path, VfsOpenFlags flags) {
	VfsAccessMode req_mode = VfsAccessMode::Read;
	if (flags == VfsOpenFlags::WriteOnly || flags == VfsOpenFlags::ReadWrite) {
		req_mode = VfsAccessMode::Write;
	}

	int32_t err = 0;
	if (!VfsPermissions::CheckAccess(guest_path, req_mode, m_mount_manager, &err)) {
		return -1; // Access denied / EROFS
	}

	std::string host_path = ResolvePath(guest_path);

	std::lock_guard<std::mutex> lock(m_mutex);
	int32_t fd = m_next_fd++;

	OpenFileInfo info{};
	info.guest_path  = guest_path;
	info.host_path   = host_path;
	info.flags       = flags;
	info.file_offset = 0;

	m_open_files[fd] = info;
	return fd;
}

bool VirtualFileSystem::CloseFile(int32_t fd) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) {
		return false;
	}
	m_open_files.erase(it);
	return true;
}

int64_t VirtualFileSystem::ReadFile(int32_t fd, void* dst, size_t size) {
	if (!dst || size == 0) return 0;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;

	std::ifstream file(it->second.host_path, std::ios::binary);
	if (!file.is_open()) {
		return 0; // EOF / Invalid host file
	}

	file.seekg(it->second.file_offset);
	file.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));
	std::streamsize read_bytes = file.gcount();
	it->second.file_offset += read_bytes;

	return static_cast<int64_t>(read_bytes);
}

int64_t VirtualFileSystem::PreadFile(int32_t fd, void* dst, size_t size, int64_t offset) {
	if (!dst || size == 0) return 0;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;

	std::ifstream file(it->second.host_path, std::ios::binary);
	if (!file.is_open()) return 0;

	file.seekg(offset);
	file.read(static_cast<char*>(dst), static_cast<std::streamsize>(size));
	return static_cast<int64_t>(file.gcount());
}

int64_t VirtualFileSystem::WriteFile(int32_t fd, const void* src, size_t size) {
	if (!src || size == 0) return 0;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;

	std::ofstream file(it->second.host_path, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open()) {
		file.open(it->second.host_path, std::ios::binary | std::ios::out);
	}
	if (!file.is_open()) return -1;

	file.seekp(it->second.file_offset);
	file.write(static_cast<const char*>(src), static_cast<std::streamsize>(size));
	it->second.file_offset += static_cast<int64_t>(size);

	return static_cast<int64_t>(size);
}

int64_t VirtualFileSystem::PwriteFile(int32_t fd, const void* src, size_t size, int64_t offset) {
	if (!src || size == 0) return 0;

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;

	std::ofstream file(it->second.host_path, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open()) return -1;

	file.seekp(offset);
	file.write(static_cast<const char*>(src), static_cast<std::streamsize>(size));
	return static_cast<int64_t>(size);
}

int64_t VirtualFileSystem::LseekFile(int32_t fd, int64_t offset, int whence) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_open_files.find(fd);
	if (it == m_open_files.end()) return -1;

	switch (whence) {
		case 0: // SEEK_SET
			it->second.file_offset = offset;
			break;
		case 1: // SEEK_CUR
			it->second.file_offset += offset;
			break;
		case 2: // SEEK_END
			if (std::filesystem::exists(it->second.host_path)) {
				it->second.file_offset = static_cast<int64_t>(std::filesystem::file_size(it->second.host_path)) + offset;
			}
			break;
		default:
			return -1;
	}
	return it->second.file_offset;
}

bool VirtualFileSystem::StatPath(const std::string& guest_path, VfsStat& out_stat) const {
	std::string host_path = ResolvePath(guest_path);
	namespace fs = std::filesystem;
	if (!fs::exists(host_path)) {
		return false;
	}

	out_stat.is_directory = fs::is_directory(host_path);
	out_stat.size_bytes   = out_stat.is_directory ? 0 : fs::file_size(host_path);
	out_stat.modified_time = 1600000000;
	out_stat.mode          = out_stat.is_directory ? 0755 : 0644;
	return true;
}

bool VirtualFileSystem::Mkdir(const std::string& guest_path, uint32_t /*mode*/) {
	int32_t err = 0;
	if (!VfsPermissions::CheckAccess(guest_path, VfsAccessMode::Write, m_mount_manager, &err)) {
		return false;
	}

	std::string host_path = ResolvePath(guest_path);
	std::error_code ec;
	return std::filesystem::create_directories(host_path, ec);
}

bool VirtualFileSystem::Rmdir(const std::string& guest_path) {
	int32_t err = 0;
	if (!VfsPermissions::CheckAccess(guest_path, VfsAccessMode::Write, m_mount_manager, &err)) {
		return false;
	}

	std::string host_path = ResolvePath(guest_path);
	std::error_code ec;
	return std::filesystem::remove(host_path, ec);
}

bool VirtualFileSystem::Unlink(const std::string& guest_path) {
	int32_t err = 0;
	if (!VfsPermissions::CheckAccess(guest_path, VfsAccessMode::Write, m_mount_manager, &err)) {
		return false;
	}

	std::string host_path = ResolvePath(guest_path);
	std::error_code ec;
	return std::filesystem::remove(host_path, ec);
}

bool VirtualFileSystem::CheckReachability(const std::string& guest_path) const {
	std::string host_path = ResolvePath(guest_path);
	return std::filesystem::exists(host_path);
}

bool VirtualFileSystem::GetDirEntries(const std::string& guest_path, std::vector<VfsDirEntry>& out_entries) const {
	std::string host_path = ResolvePath(guest_path);
	namespace fs = std::filesystem;
	if (!fs::exists(host_path) || !fs::is_directory(host_path)) {
		return false;
	}

	out_entries.clear();
	std::error_code ec;
	fs::directory_iterator it(host_path, ec);
	if (ec) {
		return false;
	}

	for (const auto& entry : it) {
		VfsDirEntry de{};
		de.name         = entry.path().filename().string();
		de.is_directory = entry.is_directory();
		de.size_bytes   = de.is_directory ? 0 : entry.file_size();
		out_entries.push_back(de);
	}
	return true;
}

} // namespace Libs::Kernel::Ps5
