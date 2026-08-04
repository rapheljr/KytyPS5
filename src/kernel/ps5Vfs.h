// ps5Vfs.h
//
// Unified PS5 Virtual Filesystem, Package Mount & Path Translation Engine for Phase N/P.

#ifndef KERNEL_PS5_VFS_H
#define KERNEL_PS5_VFS_H

#include "common/common.h"
#include "kernel/ps5PkgParser.h"
#include "kernel/ps5VfsMountManager.h"
#include "kernel/ps5VfsPathTranslator.h"
#include "kernel/ps5VfsPermissions.h"

#include <cstdint>
#include <memory>
#include <mutex>
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
	size_t   size_bytes    = 0;
	bool     is_directory  = false;
	uint64_t modified_time = 0;
	uint32_t mode          = 0755;
};

struct VfsDirEntry {
	std::string name;
	bool        is_directory = false;
	size_t      size_bytes   = 0;
};

class VirtualFileSystem {
public:
	VirtualFileSystem();
	~VirtualFileSystem() = default;

	KYTY_CLASS_NO_COPY(VirtualFileSystem);

	bool Mount(const std::string& guest_path, const std::string& host_path, MountType type = MountType::App0, MountFlags flags = MountFlags::ReadOnly, uint32_t priority = 0);
	bool Unmount(const std::string& guest_path);
	void MountStandardPs5Directories(const std::string& base_host_dir);

	bool InstallPackageBuffer(const uint8_t* pkg_data, size_t size, const std::string& target_dir, std::string* out_content_id = nullptr);

	[[nodiscard]] std::string ResolvePath(const std::string& guest_path) const;

	int32_t OpenFile(const std::string& guest_path, VfsOpenFlags flags);
	bool CloseFile(int32_t fd);
	int64_t ReadFile(int32_t fd, void* dst, size_t size);
	int64_t PreadFile(int32_t fd, void* dst, size_t size, int64_t offset);
	int64_t WriteFile(int32_t fd, const void* src, size_t size);
	int64_t PwriteFile(int32_t fd, const void* src, size_t size, int64_t offset);
	int64_t LseekFile(int32_t fd, int64_t offset, int whence);

	bool StatPath(const std::string& guest_path, VfsStat& out_stat) const;
	bool Mkdir(const std::string& guest_path, uint32_t mode = 0755);
	bool Rmdir(const std::string& guest_path);
	bool Unlink(const std::string& guest_path);
	bool CheckReachability(const std::string& guest_path) const;

	bool GetDirEntries(const std::string& guest_path, std::vector<VfsDirEntry>& out_entries) const;

	[[nodiscard]] size_t GetMountPointCount() const noexcept { return m_mount_manager.GetMountPointCount(); }
	[[nodiscard]] const VirtualMountManager& GetMountManager() const noexcept { return m_mount_manager; }

private:
	struct OpenFileInfo {
		std::string guest_path;
		std::string host_path;
		VfsOpenFlags flags;
		int64_t     file_offset = 0;
	};

	mutable std::mutex                             m_mutex;
	VirtualMountManager                            m_mount_manager;
	int32_t                                        m_next_fd = 10;
	std::unordered_map<int32_t, OpenFileInfo>       m_open_files;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_VFS_H
