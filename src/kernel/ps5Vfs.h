// ps5Vfs.h
//
// Virtual Filesystem & Package Mounting for Phase N PS5 Kernel Emulation.

#ifndef KERNEL_PS5_VFS_H
#define KERNEL_PS5_VFS_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

enum class VfsOpenFlags : uint32_t {
	ReadOnly  = 0x0000,
	WriteOnly = 0x0001,
	ReadWrite = 0x0002,
	Create    = 0x0200,
	Truncate  = 0x0400
};

struct VfsStat {
	size_t   size_bytes = 0;
	bool     is_directory = false;
	uint64_t modified_time = 0;
};

class VirtualFileSystem {
public:
	VirtualFileSystem() = default;
	~VirtualFileSystem() = default;

	KYTY_CLASS_NO_COPY(VirtualFileSystem);

	bool Mount(const std::string& guest_path, const std::string& host_path);
	bool Unmount(const std::string& guest_path);

	std::string ResolvePath(const std::string& guest_path) const;

	int32_t OpenFile(const std::string& guest_path, VfsOpenFlags flags);
	bool CloseFile(int32_t fd);
	int64_t ReadFile(int32_t fd, void* dst, size_t size);
	int64_t WriteFile(int32_t fd, const void* src, size_t size);
	bool StatPath(const std::string& guest_path, VfsStat& out_stat) const;

	[[nodiscard]] size_t GetMountPointCount() const noexcept { return m_mounts.size(); }

private:
	std::unordered_map<std::string, std::string> m_mounts;
	int32_t m_next_fd = 10;
	std::unordered_map<int32_t, std::string> m_open_files;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_VFS_H
