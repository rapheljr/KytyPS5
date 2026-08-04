// ps5VfsPathTranslator.h
//
// PS5 Path Normalization, Translation & Case-Insensitive Fallback Engine.

#ifndef KERNEL_PS5_VFS_PATH_TRANSLATOR_H
#define KERNEL_PS5_VFS_PATH_TRANSLATOR_H

#include "common/common.h"
#include "kernel/ps5VfsMountManager.h"

#include <string>

namespace Libs::Kernel::Ps5 {

class PathTranslator {
public:
	PathTranslator() = default;
	~PathTranslator() = default;

	KYTY_CLASS_NO_COPY(PathTranslator);

	static std::string NormalizePath(const std::string& raw_path);
	static std::string TranslateToHostPath(const std::string& guest_path, const VirtualMountManager& mount_mgr);
	static std::string FindCaseInsensitivePath(const std::string& host_path);
};

} // namespace Libs::Kernel::Ps5

#endif // KERNEL_PS5_VFS_PATH_TRANSLATOR_H
