// ps5VfsLayer.cpp
//
// PS5 Fast Virtual File System (VFS) Implementation.

#include "loader/ps5VfsLayer.h"

#include <algorithm>

namespace Loader {

std::string Ps5VfsLayer::NormalizeVirtualPath(const std::string& path) {
	if (path.empty()) return "/";
	std::string norm = path;
	std::replace(norm.begin(), norm.end(), '\\', '/');

	if (norm[0] != '/') {
		norm = "/" + norm;
	}

	// Remove duplicate slashes
	std::string result;
	bool last_was_slash = false;
	for (char c : norm) {
		if (c == '/') {
			if (!last_was_slash) {
				result += c;
				last_was_slash = true;
			}
		} else {
			result += c;
			last_was_slash = false;
		}
	}

	return result;
}

bool Ps5VfsLayer::Mount(const std::string& virtual_prefix, const std::string& host_path, MountPermission permission) {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::string norm_prefix = NormalizeVirtualPath(virtual_prefix);
	// Remove trailing slash from prefix if not root
	if (norm_prefix.size() > 1 && norm_prefix.back() == '/') {
		norm_prefix.pop_back();
	}

	for (auto& m : m_mounts) {
		if (m.virtual_prefix == norm_prefix) {
			m.host_path  = host_path;
			m.permission = permission;
			m_lookup_cache.clear();
			return true;
		}
	}

	VfsMountPoint mp;
	mp.virtual_prefix = norm_prefix;
	mp.host_path      = host_path;
	mp.permission     = permission;

	m_mounts.push_back(std::move(mp));
	// Sort by descending prefix length so specific mounts match before root
	std::sort(m_mounts.begin(), m_mounts.end(), [](const VfsMountPoint& a, const VfsMountPoint& b) {
		return a.virtual_prefix.size() > b.virtual_prefix.size();
	});

	m_stats.active_mount_count = static_cast<uint32_t>(m_mounts.size());
	m_lookup_cache.clear();

	return true;
}

bool Ps5VfsLayer::Unmount(const std::string& virtual_prefix) {
	std::lock_guard<std::mutex> lock(m_mutex);

	std::string norm_prefix = NormalizeVirtualPath(virtual_prefix);
	if (norm_prefix.size() > 1 && norm_prefix.back() == '/') {
		norm_prefix.pop_back();
	}

	auto it = std::remove_if(m_mounts.begin(), m_mounts.end(), [&](const VfsMountPoint& mp) {
		return mp.virtual_prefix == norm_prefix;
	});

	if (it != m_mounts.end()) {
		m_mounts.erase(it, m_mounts.end());
		m_stats.active_mount_count = static_cast<uint32_t>(m_mounts.size());
		m_lookup_cache.clear();
		return true;
	}

	return false;
}

bool Ps5VfsLayer::ResolvePath(const std::string& virtual_path, std::string& out_host_path, bool require_write_access) {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_stats.total_path_resolves++;

	std::string norm = NormalizeVirtualPath(virtual_path);

	// Check cache
	auto it_cache = m_lookup_cache.find(norm);
	if (it_cache != m_lookup_cache.end() && !require_write_access) {
		m_stats.cache_hits++;
		out_host_path = it_cache->second;
		return true;
	}

	m_stats.cache_misses++;

	for (const auto& m : m_mounts) {
		if (norm == m.virtual_prefix || norm.rfind(m.virtual_prefix + "/", 0) == 0) {
			if (require_write_access && m.permission == MountPermission::ReadOnly) {
				return false; // Permission denied
			}

			std::string suffix = norm.substr(m.virtual_prefix.size());
			if (suffix.empty()) {
				out_host_path = m.host_path;
			} else {
				if (m.host_path.back() == '/' || m.host_path.back() == '\\') {
					out_host_path = m.host_path + (suffix[0] == '/' ? suffix.substr(1) : suffix);
				} else {
					out_host_path = m.host_path + (suffix[0] == '/' ? suffix : ("/" + suffix));
				}
			}

			m_lookup_cache[norm] = out_host_path;
			return true;
		}
	}

	return false;
}

bool Ps5VfsLayer::Exists(const std::string& virtual_path) {
	std::string host_path;
	if (ResolvePath(virtual_path, host_path)) {
		return true;
	}
	return false;
}

void Ps5VfsLayer::Clear() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_mounts.clear();
	m_lookup_cache.clear();
	m_stats = {};
}

} // namespace Loader
