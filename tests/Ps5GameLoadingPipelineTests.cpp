#include "graphics/host_gpu/renderer/cache/gpuResourceManager.h"
#include "kernel/processManager.h"
#include "loader/gameLoadingPipeline.h"
#include "loader/moduleDependencyGraph.h"
#include "loader/selfParser.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace Libs::Graphics {
bool GpuResourceManager::HandleFault(PageFaultAccess, uint64_t) noexcept { return false; }
bool GpuResourceManager::InvalidateMemory(uint64_t, uint64_t) { return true; }
void GpuResourceManager::MapMemory(uint64_t, uint64_t) {}
void GpuResourceManager::UnmapMemory(uint64_t, uint64_t) {}
}

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

using namespace Loader;

struct LocalElf64Ehdr {
	unsigned char e_ident[16];
	uint16_t      e_type;
	uint16_t      e_machine;
	uint32_t      e_version;
	uint64_t      e_entry;
	uint64_t      e_phoff;
	uint64_t      e_shoff;
	uint32_t      e_flags;
	uint16_t      e_ehsize;
	uint16_t      e_phentsize;
	uint16_t      e_phnum;
	uint16_t      e_shentsize;
	uint16_t      e_shnum;
	uint16_t      e_shstrndx;
};

struct LocalElf64Phdr {
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	uint64_t p_vaddr;
	uint64_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
};

// Helper to create synthetic test directory tree with valid ELF headers
void CreateValidElfFile(const std::filesystem::path& path) {
	std::filesystem::create_directories(path.parent_path());
	LocalElf64Ehdr ehdr{};
	ehdr.e_ident[0] = 0x7f;
	ehdr.e_ident[1] = 'E';
	ehdr.e_ident[2] = 'L';
	ehdr.e_ident[3] = 'F';
	ehdr.e_ident[4] = 2; // ELFCLASS64
	ehdr.e_ident[5] = 1; // ELFDATA2LSB
	ehdr.e_ident[6] = 1; // EV_CURRENT
	ehdr.e_ident[7] = 9; // ELFOSABI_FREEBSD
	ehdr.e_ident[8] = 2; // EI_ABIVERSION (PS5 Next-Gen)
	ehdr.e_type     = 0xfe10; // ET_DYNEXEC (PS5 Executable)
	ehdr.e_machine  = 62; // EM_X86_64
	ehdr.e_version  = 1;
	ehdr.e_phoff    = sizeof(LocalElf64Ehdr);
	ehdr.e_ehsize   = sizeof(LocalElf64Ehdr);
	ehdr.e_phentsize = sizeof(LocalElf64Phdr);
	ehdr.e_phnum     = 1;
	ehdr.e_shentsize = 0;
	ehdr.e_shnum     = 0;

	LocalElf64Phdr phdr{};
	phdr.p_type   = 1; // PT_LOAD
	phdr.p_flags  = 7; // PF_R | PF_W | PF_X
	phdr.p_offset = 0;
	phdr.p_vaddr  = 0x0;
	phdr.p_paddr  = 0x0;
	phdr.p_filesz = sizeof(LocalElf64Ehdr) + sizeof(LocalElf64Phdr);
	phdr.p_memsz  = 0x1000;
	phdr.p_align  = 0x4000;

	std::ofstream ofs(path, std::ios::binary);
	ofs.write(reinterpret_cast<const char*>(&ehdr), sizeof(ehdr));
	ofs.write(reinterpret_cast<const char*>(&phdr), sizeof(phdr));
}

void CreateTestFile(const std::filesystem::path& path, const std::string& content) {
	std::filesystem::create_directories(path.parent_path());
	std::ofstream ofs(path, std::ios::binary);
	ofs.write(content.data(), content.size());
}

// ─── 1. SELF Parser Test ──────────────────────────────────────────────────────

void TestSelfParser() {
	std::printf("  [Test 1] PS5 Signed ELF (SELF) Header & Segment Parser...\n");

	std::vector<uint8_t> dummy_self(sizeof(SelfHeader) + sizeof(SelfSegmentHeader) * 2 + 100, 0);

	SelfHeader hdr{};
	hdr.magic         = kSelfMagic; // 0x4F534C46
	hdr.version       = 1;
	hdr.header_size   = sizeof(SelfHeader) + sizeof(SelfSegmentHeader) * 2;
	hdr.segment_count = 2;
	hdr.file_size     = dummy_self.size();

	std::memcpy(dummy_self.data(), &hdr, sizeof(SelfHeader));

	SelfSegmentHeader seg0{0x1, 0x100, 0x50, 0x50};
	SelfSegmentHeader seg1{0x2, 0x150, 0x50, 0x50};

	std::memcpy(dummy_self.data() + sizeof(SelfHeader), &seg0, sizeof(SelfSegmentHeader));
	std::memcpy(dummy_self.data() + sizeof(SelfHeader) + sizeof(SelfSegmentHeader), &seg1, sizeof(SelfSegmentHeader));

	CHECK(SelfParser::IsSelfBuffer(dummy_self.data(), dummy_self.size()));

	SelfInfo info;
	CHECK(SelfParser::Parse(dummy_self.data(), dummy_self.size(), info));
	CHECK(info.valid);
	CHECK_EQ(info.header.magic, kSelfMagic);
	CHECK_EQ(info.segments.size(), 2u);

	std::vector<uint8_t> extracted;
	CHECK(SelfParser::ExtractElf(dummy_self.data(), dummy_self.size(), extracted));
	CHECK_EQ(extracted.size(), 100u);

	std::printf("  [OK] Test 1: PS5 Signed ELF (SELF) Header & Segment Parser\n");
}

// ─── 2. Module Dependency Graph Test ─────────────────────────────────────────

void TestModuleDependencyGraph() {
	std::printf("  [Test 2] Dynamic Module Dependency Graph & Topological Sorting...\n");

	ModuleDependencyGraph graph;
	graph.AddModule("libSceKernel.sprx", nullptr, {});
	graph.AddModule("libScePad.sprx", nullptr, {"libSceKernel.sprx"});
	graph.AddModule("libSceAudio.sprx", nullptr, {"libSceKernel.sprx"});
	graph.AddModule("eboot.bin", nullptr, {"libScePad.sprx", "libSceAudio.sprx"});

	CHECK(graph.BuildGraph());

	std::string diag = graph.GetDiagnostics();
	CHECK(diag.find("eboot.bin") != std::string::npos);
	CHECK_EQ(graph.GetModuleCount(), 4u);

	// Test cycle detection
	ModuleDependencyGraph cycle_graph;
	cycle_graph.AddModule("A", nullptr, {"B"});
	cycle_graph.AddModule("B", nullptr, {"C"});
	cycle_graph.AddModule("C", nullptr, {"A"});
	cycle_graph.BuildGraph();
	CHECK(cycle_graph.DetectCycles());

	std::printf("  [OK] Test 2: Dynamic Module Dependency Graph & Topological Sorting\n");
}

// ─── 3. PKG, Patch Overlay & DLC Mount Test ──────────────────────────────────

void TestPkgPatchAndDlcMounts() {
	std::printf("  [Test 3] Game Update Overlays (/patch) & DLC Mounts (/addcont)...\n");

	std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "kyty_pipeline_test";
	std::filesystem::remove_all(temp_dir);

	CreateValidElfFile(temp_dir / "app0" / "eboot.bin");
	CreateTestFile(temp_dir / "app0" / "data.bin", "APP0_DATA");
	CreateValidElfFile(temp_dir / "patch" / "eboot.bin"); // Patch overlay
	CreateTestFile(temp_dir / "dlc0" / "extra.bin", "DLC_EXTRA");

	GameLoadingPipeline pipeline;
	CHECK(pipeline.Initialize());

	GamePipelineConfig config{};
	config.game_path  = temp_dir / "app0";
	config.patch_path = temp_dir / "patch";
	config.dlc_paths  = {temp_dir / "dlc0"};

	CHECK(pipeline.LoadGame(config));

	const auto& diag = pipeline.GetDiagnostics();
	CHECK(diag.status == GamePipelineStatus::Loaded);
	CHECK(diag.mounted_files_count >= 2);

	// Verify path resolution via VFS
	auto path_res = pipeline.GetVfsManager()->ResolvePath("/app0/eboot.bin");
	CHECK_EQ(path_res, (temp_dir / "patch" / "eboot.bin").string());

	pipeline.Shutdown();
	std::filesystem::remove_all(temp_dir);

	std::printf("  [OK] Test 3: Game Update Overlays (/patch) & DLC Mounts (/addcont)\n");
}

// ─── 4. Kernel Startup Integration Test ──────────────────────────────────────

void TestKernelStartupIntegration() {
	std::printf("  [Test 4] Kernel Startup & Guest Process Spawn Integration...\n");

	GameLoadingPipeline pipeline;
	CHECK(pipeline.Initialize());

	std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "kyty_kernel_start_test";
	CreateValidElfFile(temp_dir / "eboot.bin");

	GamePipelineConfig config{};
	config.game_path = temp_dir;
	pipeline.LoadGame(config);

	CHECK(pipeline.ExecuteStartup());

	pipeline.Shutdown();
	std::filesystem::remove_all(temp_dir);

	std::printf("  [OK] Test 4: Kernel Startup & Guest Process Spawn Integration\n");
}

// ─── Benchmarks ───────────────────────────────────────────────────────────────

void BenchmarkGameLoadingPipeline() {
	std::printf("\n--- PS5 Game Loading Pipeline Benchmarks ---\n");

	ModuleDependencyGraph graph;
	constexpr int kModuleCount = 100;

	for (int i = 0; i < kModuleCount; ++i) {
		std::string name = "module_" + std::to_string(i);
		std::vector<std::string> deps;
		if (i > 0) deps.push_back("module_" + std::to_string(i - 1));
		graph.AddModule(name, nullptr, deps);
	}
	graph.BuildGraph();

	constexpr int kRuns = 10000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kRuns; ++i) {
		(void)graph.GetInitializationOrder();
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double dt_us = std::chrono::duration<double, std::micro>(t1 - t0).count() / kRuns;
	double throughput = (kRuns * kModuleCount) / std::chrono::duration<double>(t1 - t0).count();

	std::printf("  [Bench] 100-Module Dependency Graph Topological Sort: %.2f us / run (Throughput: %.2f M modules/sec)\n",
	           dt_us, throughput / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5: Complete Game Loading Pipeline Test Suite \n");
	std::printf("====================================================\n\n");

	TestSelfParser();
	TestModuleDependencyGraph();
	TestPkgPatchAndDlcMounts();
	TestKernelStartupIntegration();

	BenchmarkGameLoadingPipeline();

	std::printf("\n====================================================\n");
	std::printf(" Results: %d/%d tests passed", g_tests_passed, g_tests_run);
	if (g_tests_failed > 0) {
		std::printf(" — %d FAILED\n", g_tests_failed);
		std::printf("Ps5GameLoadingPipelineTests: FAILED\n");
		return 1;
	}
	std::printf("\nPs5GameLoadingPipelineTests: ALL PASSED\n");
	return 0;
}
