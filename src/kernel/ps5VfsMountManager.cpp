// ps5VfsMountManager.cpp
//
// PS5 Virtual Mount Manager & Patch Overlay Resolution Engine Implementation.

#include "kernel/ps5VfsMountManager.h"

#include <algorithm>
#include <filesystem>

namespace Libs::Kernel::Ps5 {

VirtualMountManager::VirtualMountManager() = default;

bool VirtualMountManager::Mount(const std::string& guest_path, const std::string& host_path, MountType type, MountFlags flags, uint32_t priority) {
	if (guest_path.empty() || host_path.empty()) return false;

	std::lock_guard<std::mutex> lock(m_mutex);

	// Remove existing mount on same guest_path if exact match
	m_mounts.erase(std::remove_if(m_mounts.begin(), m_mounts.end(),
	                              [&guest_path](const MountPoint& mp) { return mp.guest_path == guest_path; }),
	               m_mounts.end());

	MountPoint mp{};
	mp.guest_path = guest_path;
	mp.host_path  = host_path;
	mp.type       = type;
	mp.flags      = flags;
	mp.priority   = priority;

	m_mounts.push_back(mp);

	// Sort mounts by guest_path length descending, then priority descending
	std::sort(m_mounts.begin(), m_mounts.end(), [](const MountPoint& a, const MountPoint& b) {
		if (a.guest_path.length() != b.guest_path.length()) {
			return a.guest_path.length() > b.guest_path.length();
		}
		return a.priority > b.priority;
	});

	return true;
}

bool VirtualMountManager::Unmount(const std::string& guest_path) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto initial_size = m_mounts.size();
	m_mounts.erase(std::remove_if(m_mounts.begin(), m_mounts.end(),
	                              [&guest_path](const MountPoint& mp) { return mp.guest_path == guest_path; }),
	               m_mounts.end());
	return m_mounts.size() < initial_size;
}

std::string VirtualMountManager::ResolveHostPath(const std::string& guest_path) const {
	std::lock_guard<std::mutex> lock(m_mutex);

	// Check patch overlay for /app0 files first
	if (guest_path.rfind("/app0/", 0) == 0) {
		std::string rel_path = guest_path.substr(5);
		// Check if a patch mount exists and contains the file
		for (const auto& mp : m_mounts) {
			if (mp.type == MountType::Patch) {
				std::string candidate = mp.host_path + rel_path;
				if (std::filesystem::exists(candidate)) {
					return candidate;
				}
			}
		}
	}

	for (const auto& mp : m_mounts) {
		if (guest_path.rfind(mp.guest_path, 0) == 0) {
			std::string sub = guest_path.substr(mp.guest_path.length());
			if (sub.empty() || sub[0] == '/' || mp.guest_path.back() == '/') {
				return mp.host_path + sub;
			}
		}
	}

	return guest_path; // Fallback to raw guest path
}

const MountPoint* VirtualMountManager::GetMountPointForPath(const std::string& guest_path) const {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (const auto& mp : m_mounts) {
		if (guest_path.rfind(mp.guest_path, 0) == 0) {
			return &mp;
		}
	}
	return nullptr;
}

void VirtualMountManager::MountStandardPs5Directories(const std::string& base_host_dir) {
	std::string base = base_host_dir;
	if (!base.empty() && base.back() != '/') base += '/';

	Mount("/app0",      base + "app0",      MountType::App0,     MountFlags::ReadOnly,  10);
	Mount("/patch",     base + "patch",     MountType::Patch,    MountFlags::ReadOnly,  20);
	Mount("/addcont0",  base + "addcont0",  MountType::DLC,      MountFlags::ReadOnly,  10);
	Mount("/savedata",  base + "savedata",  MountType::SaveData, MountFlags::ReadWrite, 10);
	Mount("/temp",      base + "temp",      MountType::Temp,     MountFlags::ReadWrite, 10);
	Mount("/cache",     base + "cache",     MountType::Cache,    MountFlags::ReadWrite, 10);
	Mount("/trophy",    base + "trophy",    MountType::Trophy,   MountFlags::ReadWrite, 10);
	Mount("/dev",       base + "dev",       MountType::Dev,      MountFlags::ReadOnly,  10);
	Mount("/system",    base + "system",    MountType::System,   MountFlags::ReadOnly,  10);
}

size_t VirtualMountManager::GetMountPointCount() const {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_mounts.size();
}

} // namespace Libs::Kernel::Ps5
