// ps5VfsLayer.h
//
// PS5 Fast Virtual File System (VFS) & Mount Overlay for KytyPS5.
// Translates virtual paths (/app0, /data, /savedata0, /system_data) to host filesystem paths
// with fast case-insensitive caching and sandboxed permissions.

#ifndef LOADER_PS5_VFS_LAYER_H
#define LOADER_PS5_VFS_LAYER_H

#include "common/common.h"

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Loader {

enum class MountPermission : uint8_t {
	ReadOnly,
	ReadWrite
};

struct VfsMountPoint {
	std::string     virtual_prefix; // e.g. "/app0"
	std::string     host_path;      // e.g. "/Users/.../extracted_pkg"
	MountPermission permission = MountPermission::ReadOnly;
};

struct VfsStats {
	uint64_t total_path_resolves = 0;
	uint64_t cache_hits          = 0;
	uint64_t cache_misses        = 0;
	uint32_t active_mount_count  = 0;
};

class Ps5VfsLayer {
public:
	Ps5VfsLayer() = default;
	~Ps5VfsLayer() = default;

	KYTY_CLASS_NO_COPY(Ps5VfsLayer);

	/// Mount a virtual path to a host directory
	bool Mount(const std::string& virtual_prefix, const std::string& host_path, MountPermission permission = MountPermission::ReadOnly);

	/// Unmount a virtual path
	bool Unmount(const std::string& virtual_prefix);

	/// Resolve virtual path to physical host path
	bool ResolvePath(const std::string& virtual_path, std::string& out_host_path, bool require_write_access = false);

	/// Check if virtual path exists
	bool Exists(const std::string& virtual_path);

	[[nodiscard]] const VfsStats& GetStats() const noexcept { return m_stats; }
	void Clear() noexcept;

	static std::string NormalizeVirtualPath(const std::string& path);

private:
	std::vector<VfsMountPoint>                    m_mounts;
	std::unordered_map<std::string, std::string>  m_lookup_cache;
	std::mutex                                    m_mutex;
	VfsStats                                      m_stats{};
};

} // namespace Loader

#endif // LOADER_PS5_VFS_LAYER_H
