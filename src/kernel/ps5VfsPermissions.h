// ps5VfsPermissions.h
//
// PS5 Filesystem Permissions & Access Control Engine.

#ifndef KERNEL_PS5_VFS_PERMISSIONS_H
#define KERNEL_PS5_VFS_PERMISSIONS_H

#include "common/common.h"
#include "kernel/ps5VfsMountManager.h"

#include <cstdint>
#include <string>

namespace Libs::Kernel::Ps5 {

enum class VfsAccessMode : uint32_t {
	Execute = 1 << 0,
	Write   = 1 << 1,
	Read    = 1 << 2
};

inline VfsAccessMode operator|(VfsAccessMode a, VfsAccessMode b) {
	return static_cast<VfsAccessMode>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool operator&(VfsAccessMode a, VfsAccessMode b) {
	return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

class VfsPermissions {
public:
	VfsPermissions() = default;
	~VfsPermissions() = default;

	KYTY_CLASS_NO_COPY(VfsPermissions);

	static bool CheckAccess(const std::string& guest_path, VfsAccessMode mode, const VirtualMountManager& mount_mgr, int32_t* out_errno = nullptr);
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_VFS_PERMISSIONS_H
