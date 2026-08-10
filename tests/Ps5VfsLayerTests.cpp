// Ps5VfsLayerTests.cpp
//
// Unit & Integration Tests for PS5 Virtual File System (VFS) Layer.

#include "loader/ps5VfsLayer.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace Loader;

static void TestVfsMountAndResolve() {
	std::printf("[TEST] Ps5Vfs_MountAndResolve\n");

	Ps5VfsLayer vfs;
	vfs.Mount("/app0", "/opt/kyty/games/CUSA00001", MountPermission::ReadOnly);
	vfs.Mount("/savedata0", "/opt/kyty/users/10000000/savedata", MountPermission::ReadWrite);

	std::string resolved;
	// 1. Read game executable from /app0
	if (!vfs.ResolvePath("/app0/eboot.bin", resolved) || resolved != "/opt/kyty/games/CUSA00001/eboot.bin") {
		std::fprintf(stderr, "FAIL: /app0/eboot.bin resolved incorrectly to: %s\n", resolved.c_str());
		std::exit(1);
	}

	// 2. Read user save data from /savedata0
	if (!vfs.ResolvePath("/savedata0/save.dat", resolved) || resolved != "/opt/kyty/users/10000000/savedata/save.dat") {
		std::fprintf(stderr, "FAIL: /savedata0/save.dat resolved incorrectly to: %s\n", resolved.c_str());
		std::exit(1);
	}

	// 3. Cache verification
	vfs.ResolvePath("/app0/eboot.bin", resolved);
	const auto& stats = vfs.GetStats();
	if (stats.cache_hits != 1 || stats.total_path_resolves != 3) {
		std::fprintf(stderr, "FAIL: Cache hit count mismatch (Hits=%llu, Total=%llu)\n",
		             stats.cache_hits, stats.total_path_resolves);
		std::exit(1);
	}

	std::printf("  [ OK ] Ps5Vfs_MountAndResolve\n");
}

static void TestVfsPermissionsAndUnmount() {
	std::printf("[TEST] Ps5Vfs_PermissionsAndUnmount\n");

	Ps5VfsLayer vfs;
	vfs.Mount("/app0", "/opt/kyty/games/CUSA00001", MountPermission::ReadOnly);

	std::string resolved;
	// Attempt write on ReadOnly mount
	if (vfs.ResolvePath("/app0/eboot.bin", resolved, /*require_write_access=*/true)) {
		std::fprintf(stderr, "FAIL: ReadOnly mount permitted write access\n");
		std::exit(1);
	}

	// Unmount
	if (!vfs.Unmount("/app0")) {
		std::fprintf(stderr, "FAIL: Unmount /app0 failed\n");
		std::exit(1);
	}

	if (vfs.ResolvePath("/app0/eboot.bin", resolved)) {
		std::fprintf(stderr, "FAIL: Path resolved after unmount\n");
		std::exit(1);
	}

	std::printf("  [ OK ] Ps5Vfs_PermissionsAndUnmount\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — PS5 Virtual File System (VFS) Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestVfsMountAndResolve();
	TestVfsPermissionsAndUnmount();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
