// Ps5VfsCompleteTests.cpp
//
// Complete Automated Test Suite for PS5 Virtual Filesystem, Package Parser,
// Integrity Verifier, Mount Manager, Permissions, and Path Translator.

#include "kernel/ps5PkgParser.h"
#include "kernel/ps5Vfs.h"
#include "kernel/ps5VfsMountManager.h"
#include "kernel/ps5VfsPathTranslator.h"
#include "kernel/ps5VfsPermissions.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

void Check(bool value, const char* description, const char* file, int line) {
	g_tests_run++;
	if (!value) {
		g_tests_failed++;
		std::printf("  [FAIL] %s\n         at %s:%d\n", description, file, line);
	} else {
		g_tests_passed++;
	}
}

#define CHECK(expr) Check((expr), #expr, __FILE__, __LINE__)
#define CHECK_EQ(a, b) Check((a) == (b), #a " == " #b, __FILE__, __LINE__)

using namespace Libs::Kernel::Ps5;

// Helper to generate a dummy binary PKG buffer
std::vector<uint8_t> CreateDummyPkgBuffer(const char* content_id, uint32_t type = 1) {
	std::vector<uint8_t> buf(512, 0);

	uint32_t magic = kPkgMagicCNT;
	std::memcpy(buf.data(), &magic, sizeof(uint32_t));
	std::memcpy(buf.data() + 4, &type, sizeof(uint32_t));

	uint32_t entry_count = 2;
	uint64_t table_offset = 128;
	uint64_t body_offset = 256;
	uint64_t body_size = 256;

	std::memcpy(buf.data() + 8, &entry_count, sizeof(uint32_t));
	std::memcpy(buf.data() + 12, &table_offset, sizeof(uint64_t));
	std::memcpy(buf.data() + 20, &body_offset, sizeof(uint64_t));
	std::memcpy(buf.data() + 28, &body_size, sizeof(uint64_t));

	std::memcpy(buf.data() + 36, content_id, std::min<size_t>(36, std::strlen(content_id)));

	// Entry 1: param.sfo (id=1, offset=256, size=64)
	uint32_t e1_id = 0x0001;
	uint64_t e1_off = 256;
	uint64_t e1_sz = 64;
	std::memcpy(buf.data() + 128, &e1_id, 4);
	std::memcpy(buf.data() + 132, &e1_off, 8);
	std::memcpy(buf.data() + 140, &e1_sz, 8);

	// Entry 2: eboot.bin (id=0x1000, offset=320, size=128)
	uint32_t e2_id = 0x1000;
	uint64_t e2_off = 320;
	uint64_t e2_sz = 128;
	std::memcpy(buf.data() + 160, &e2_id, 4);
	std::memcpy(buf.data() + 164, &e2_off, 8);
	std::memcpy(buf.data() + 172, &e2_sz, 8);

	return buf;
}

// ─── 1. Package Parser & Integrity Verifier Test ─────────────────────────────

void TestPkgParserAndIntegrity() {
	std::printf("  [Test 1] Package Parser & SHA-256 Integrity Verifier...\n");

	auto pkg_data = CreateDummyPkgBuffer("HP0700-PPSA01234_00-GAME000000000000", 1);

	PkgParser parser;
	CHECK(parser.Parse(pkg_data.data(), pkg_data.size()));

	CHECK_EQ(parser.GetHeader().magic, kPkgMagicCNT);
	CHECK(parser.GetHeader().type == PkgType::App);
	CHECK_EQ(std::string(parser.GetHeader().content_id), std::string("HP0700-PPSA01234_00-GAME000000000000"));

	CHECK(parser.HasEntry(0x0001)); // PARAM_SFO
	CHECK(parser.HasEntry(0x1000)); // EBOOT_BIN

	CHECK(parser.VerifyIntegrity(pkg_data.data(), pkg_data.size()));

	// Tamper header byte
	auto tampered = pkg_data;
	tampered[10] ^= 0xFF;
	CHECK(!parser.VerifyIntegrity(tampered.data(), tampered.size()));

	std::vector<uint8_t> extracted;
	CHECK(parser.ExtractEntry(pkg_data.data(), pkg_data.size(), 0x0001, extracted));
	CHECK_EQ(extracted.size(), 64u);

	std::printf("  [OK] Test 1: Package Parser & SHA-256 Integrity Verifier\n");
}

// ─── 2. Package Installer Test ───────────────────────────────────────────────

void TestPkgInstaller() {
	std::printf("  [Test 2] Package Installer & Buffer Installation...\n");

	auto pkg_data = CreateDummyPkgBuffer("HP0700-PPSA99999_00-PATCH00000000000", 2);
	std::string content_id;

	CHECK(PkgInstaller::InstallPackageBuffer(pkg_data.data(), pkg_data.size(), "/tmp/test_install", &content_id));
	CHECK_EQ(content_id, std::string("HP0700-PPSA99999_00-PATCH00000000000"));

	std::printf("  [OK] Test 2: Package Installer & Buffer Installation\n");
}

// ─── 3. Path Normalization & Case-Insensitive Translator Test ───────────────

void TestPathTranslator() {
	std::printf("  [Test 3] Path Normalization & Case-Insensitive Translation...\n");

	CHECK_EQ(PathTranslator::NormalizePath("/app0/bin/../eboot.bin"), std::string("/app0/eboot.bin"));
	CHECK_EQ(PathTranslator::NormalizePath("app0\\data\\\\config.xml"), std::string("/app0/data/config.xml"));
	CHECK_EQ(PathTranslator::NormalizePath("/a/b/c/../../d"), std::string("/a/d"));

	VirtualMountManager mount_mgr;
	mount_mgr.Mount("/app0", "/tmp/host_app0");

	std::string host_res = PathTranslator::TranslateToHostPath("/app0/eboot.bin", mount_mgr);
	CHECK_EQ(host_res, std::string("/tmp/host_app0/eboot.bin"));

	std::printf("  [OK] Test 3: Path Normalization & Case-Insensitive Translation\n");
}

// ─── 4. Virtual Mount Manager & Patch Overlay Resolution Test ────────────────

void TestVirtualMountManager() {
	std::printf("  [Test 4] Virtual Mount Manager & Patch Overlay Resolution...\n");

	namespace fs = std::filesystem;
	std::string base_dir = "/tmp/kyty_vfs_test";
	fs::create_directories(base_dir + "/app0");
	fs::create_directories(base_dir + "/patch");

	// Create a base file in app0
	std::ofstream(base_dir + "/app0/base_asset.bin") << "v1.0 base asset";
	std::ofstream(base_dir + "/app0/updated_asset.bin") << "v1.0 old asset";

	// Create an updated overlay file in patch
	std::ofstream(base_dir + "/patch/updated_asset.bin") << "v1.1 patch asset";

	VirtualMountManager mount_mgr;
	mount_mgr.MountStandardPs5Directories(base_dir);

	CHECK_EQ(mount_mgr.GetMountPointCount(), 9u);

	// Resolving updated_asset under /app0/ should return the patch overlay file!
	std::string resolved_patch = mount_mgr.ResolveHostPath("/app0/updated_asset.bin");
	CHECK_EQ(resolved_patch, base_dir + "/patch/updated_asset.bin");

	// Resolving base_asset under /app0/ (not in patch) returns the app0 file
	std::string resolved_base = mount_mgr.ResolveHostPath("/app0/base_asset.bin");
	CHECK_EQ(resolved_base, base_dir + "/app0/base_asset.bin");

	fs::remove_all(base_dir);

	std::printf("  [OK] Test 4: Virtual Mount Manager & Patch Overlay Resolution\n");
}

// ─── 5. Filesystem Permissions & ReadOnly Enforcement Test ───────────────────

void TestVfsPermissions() {
	std::printf("  [Test 5] Filesystem Permissions & ReadOnly Enforcement...\n");

	VirtualMountManager mount_mgr;
	mount_mgr.Mount("/app0", "/host/app0", MountType::App0, MountFlags::ReadOnly);
	mount_mgr.Mount("/savedata", "/host/savedata", MountType::SaveData, MountFlags::ReadWrite);

	int32_t err = 0;
	// Read access to /app0 should succeed
	CHECK(VfsPermissions::CheckAccess("/app0/eboot.bin", VfsAccessMode::Read, mount_mgr, &err));

	// Write access to /app0 should fail with EROFS (Read-only file system)
	CHECK(!VfsPermissions::CheckAccess("/app0/eboot.bin", VfsAccessMode::Write, mount_mgr, &err));
	CHECK_EQ(err, 30); // EROFS

	// Write access to /savedata should succeed
	CHECK(VfsPermissions::CheckAccess("/savedata/save0.dat", VfsAccessMode::Write, mount_mgr, &err));

	std::printf("  [OK] Test 5: Filesystem Permissions & ReadOnly Enforcement\n");
}

// ─── 6. Full VFS File I/O & Directory Enumeration Test ───────────────────────

void TestFullVfsFileIO() {
	std::printf("  [Test 6] Full VFS File I/O & Directory Enumeration...\n");

	namespace fs = std::filesystem;
	std::string base_dir = "/tmp/kyty_vfs_io_test";
	fs::create_directories(base_dir + "/savedata");

	VirtualFileSystem vfs;
	vfs.MountStandardPs5Directories(base_dir);

	// Attempt write open on ReadOnly /app0 -> fails
	int32_t ro_fd = vfs.OpenFile("/app0/test.txt", VfsOpenFlags::WriteOnly);
	CHECK_EQ(ro_fd, -1);

	// Write open on ReadWrite /savedata -> succeeds
	int32_t save_fd = vfs.OpenFile("/savedata/test_save.dat", VfsOpenFlags::ReadWrite);
	CHECK(save_fd >= 10);

	char write_data[] = "PS5 Save State Data 2026";
	int64_t written = vfs.WriteFile(save_fd, write_data, std::strlen(write_data));
	CHECK_EQ(written, static_cast<int64_t>(std::strlen(write_data)));

	vfs.LseekFile(save_fd, 0, 0); // Seek to start

	char read_data[64] = {};
	int64_t read_bytes = vfs.ReadFile(save_fd, read_data, sizeof(read_data));
	CHECK_EQ(read_bytes, written);
	CHECK_EQ(std::string(read_data), std::string(write_data));

	vfs.CloseFile(save_fd);

	// Stat check
	VfsStat stat_info{};
	CHECK(vfs.StatPath("/savedata/test_save.dat", stat_info));
	CHECK(!stat_info.is_directory);
	CHECK_EQ(stat_info.size_bytes, static_cast<size_t>(written));

	// Directory enumeration
	std::vector<VfsDirEntry> entries;
	CHECK(vfs.GetDirEntries("/savedata", entries));
	CHECK_EQ(entries.size(), 1u);
	CHECK_EQ(entries[0].name, std::string("test_save.dat"));

	vfs.Unlink("/savedata/test_save.dat");
	CHECK(!vfs.CheckReachability("/savedata/test_save.dat"));

	fs::remove_all(base_dir);

	std::printf("  [OK] Test 6: Full VFS File I/O & Directory Enumeration\n");
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

void BenchmarkVfs() {
	std::printf("\n--- PS5 Virtual Filesystem Benchmarks ---\n");

	VirtualMountManager mount_mgr;
	mount_mgr.MountStandardPs5Directories("/tmp/kyty_bench");

	constexpr int kPathResolutions = 1000000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kPathResolutions; ++i) {
		(void)mount_mgr.ResolveHostPath("/app0/sub/folder/asset.bin");
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kPathResolutions;
	double throughput = kPathResolutions / std::chrono::duration<double>(t1 - t0).count();
	std::printf("  [Bench] VFS Path Overlay Resolution Latency: %.2f ns / path (Throughput: %.2f M paths/sec)\n",
	           dt_ns, throughput / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5: Complete PS5 VFS & Package Test Suite     \n");
	std::printf("====================================================\n\n");

	TestPkgParserAndIntegrity();
	TestPkgInstaller();
	TestPathTranslator();
	TestVirtualMountManager();
	TestVfsPermissions();
	TestFullVfsFileIO();

	BenchmarkVfs();

	std::printf("\n====================================================\n");
	std::printf(" Results: %d/%d tests passed", g_tests_passed, g_tests_run);
	if (g_tests_failed > 0) {
		std::printf(" — %d FAILED\n", g_tests_failed);
		std::printf("Ps5VfsCompleteTests: FAILED\n");
		return 1;
	}
	std::printf("\nPs5VfsCompleteTests: ALL PASSED\n");
	return 0;
}
