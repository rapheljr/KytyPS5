// ps5VfsPermissions.cpp
//
// PS5 Filesystem Permissions & Access Control Engine Implementation.

#include "kernel/ps5VfsPermissions.h"

namespace Libs::Kernel::Ps5 {

constexpr int32_t KERNEL_EACCES = 13; // Permission denied
constexpr int32_t KERNEL_EROFS  = 30; // Read-only file system

bool VfsPermissions::CheckAccess(const std::string& guest_path, VfsAccessMode mode, const VirtualMountManager& mount_mgr, int32_t* out_errno) {
	const MountPoint* mp = mount_mgr.GetMountPointForPath(guest_path);
	if (!mp) {
		if (out_errno) *out_errno = KERNEL_EACCES;
		return false;
	}

	// Check Read-Only mount enforcement on Write requests
	if (mode & VfsAccessMode::Write) {
		if (mp->flags & MountFlags::ReadOnly) {
			if (out_errno) *out_errno = KERNEL_EROFS;
			return false; // Write denied on read-only mount
		}
	}

	if (out_errno) *out_errno = 0;
	return true;
}

} // namespace Libs::Kernel::Ps5
