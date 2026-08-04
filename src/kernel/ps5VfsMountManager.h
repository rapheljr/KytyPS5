// ps5VfsMountManager.h
//
// PS5 Virtual Mount Manager & Patch Overlay Resolution Engine.

#ifndef KERNEL_PS5_VFS_MOUNT_MANAGER_H
#define KERNEL_PS5_VFS_MOUNT_MANAGER_H

#include "common/common.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libs::Kernel::Ps5 {

enum class MountType : uint8_t {
	App0 = 0,
	Patch,
	DLC,
	SaveData,
	Temp,
	Cache,
	Trophy,
	Dev,
	System
};

enum class MountFlags : uint32_t {
	None      = 0,
	ReadOnly  = 1 << 0,
	ReadWrite = 1 << 1,
	Overlay   = 1 << 2
};

inline MountFlags operator|(MountFlags a, MountFlags b) {
	return static_cast<MountFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(MountFlags a, MountFlags b) {
	return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

struct MountPoint {
	std::string guest_path;
	std::string host_path;
	MountType   type     = MountType::App0;
	MountFlags  flags    = MountFlags::ReadOnly;
	uint32_t    priority = 0;
};

class VirtualMountManager {
public:
	VirtualMountManager();
	~VirtualMountManager() = default;

	KYTY_CLASS_NO_COPY(VirtualMountManager);

	bool Mount(const std::string& guest_path, const std::string& host_path, MountType type = MountType::App0, MountFlags flags = MountFlags::ReadOnly, uint32_t priority = 0);
	bool Unmount(const std::string& guest_path);

	[[nodiscard]] std::string ResolveHostPath(const std::string& guest_path) const;
	[[nodiscard]] const MountPoint* GetMountPointForPath(const std::string& guest_path) const;

	void MountStandardPs5Directories(const std::string& base_host_dir);

	[[nodiscard]] size_t GetMountPointCount() const;

private:
	mutable std::mutex       m_mutex;
	std::vector<MountPoint>  m_mounts;
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_VFS_MOUNT_MANAGER_H
