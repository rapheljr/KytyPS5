// CompatibilityFrameworkTests.cpp
//
// Unit, integration, and benchmark test suite for Phase P: Compatibility Framework.

#include "emulator/compat/compatFramework.h"
#include "emulator/compat/developerTools.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Emulator::Compat;

// ─── 1. Game & Compatibility Database Test ───────────────────────────────────

void TestGameAndCompatibilityDatabase() {
	std::printf("  [Test 1] Game & Compatibility Database Lookup...\n");

	GameDatabase db;
	const GameInfo* info = db.LookupGame("CUSA00001");
	Check(info != nullptr, "Title CUSA00001 lookup failed");
	Check(info->rating == CompatibilityRating::Playable, "Title rating mismatch");

	GameInfo custom{};
	custom.title_id   = "PPSA99999";
	custom.title_name = "Custom PS5 Game";
	custom.rating     = CompatibilityRating::Ingame;
	db.RegisterGame(custom);

	const GameInfo* custom_info = db.LookupGame("PPSA99999");
	Check(custom_info != nullptr, "Custom title PPSA99999 lookup failed");
	Check(custom_info->rating == CompatibilityRating::Ingame, "Custom title rating mismatch");
	Check(db.GetGameCount() == 2, "Database game count mismatch");

	std::printf("  [OK] Test 1: Game & Compatibility Database Lookup\n");
}

// ─── 2. Persistent Shader Cache Test ──────────────────────────────────────────

void TestPersistentShaderCache() {
	std::printf("  [Test 2] Persistent Shader Cache Serialization & Hit Rate...\n");

	PersistentShaderCache cache("/tmp/kyty_shader_test_cache");
	uint64_t shader_hash = 0xABCD1234EF567890ULL;
	std::vector<uint8_t> msl_code = {'m', 'a', 'i', 'n', '(', ')'};

	bool store_ok = cache.StoreShader(shader_hash, msl_code);
	Check(store_ok, "Shader store failed");

	std::vector<uint8_t> loaded_code;
	bool load_ok = cache.LoadShader(shader_hash, loaded_code);
	Check(load_ok, "Shader load failed");
	Check(loaded_code == msl_code, "Loaded shader binary mismatch");

	std::printf("  [OK] Test 2: Persistent Shader Cache\n");
}

// ─── 3. Crash Reporter & Minidump Test ────────────────────────────────────────

void TestCrashReporter() {
	std::printf("  [Test 3] Crash Reporter & Minidump Log Generation...\n");

	CrashReport report = CrashReporter::CreateReport("CUSA00001", 0x0000000000401050ULL, "SIGSEGV");
	Check(report.title_id == "CUSA00001", "Crash report title ID mismatch");
	Check(report.fault_address == 0x0000000000401050ULL, "Crash report fault address mismatch");

	std::string filepath = "/tmp/kyty_test_crash.log";
	bool save_ok = CrashReporter::SaveReportToFile(report, filepath);
	Check(save_ok, "Crash report save to file failed");

	std::remove(filepath.c_str());

	std::printf("  [OK] Test 3: Crash Reporter & Minidump Log Generation\n");
}

// ─── 4. Developer Inspection Tools Test ───────────────────────────────────────

void TestDeveloperTools() {
	std::printf("  [Test 4] Developer Inspection Tools (FrameDebugger, ResourceInspector)...\n");

	FrameDebugger frame_dbg;
	frame_dbg.RecordDrawCall(36, "VS_Main", "PS_Main");
	frame_dbg.RecordDrawCall(128, "VS_Shadow", "PS_Shadow");

	Check(frame_dbg.GetDrawCallCount() == 2, "Draw call count mismatch");
	const DrawCallInfo* draw0 = frame_dbg.GetDrawCall(0);
	Check(draw0 != nullptr && draw0->index_count == 36, "Draw 0 info mismatch");

	ResourceUsage usage = ResourceInspector::QueryCurrentUsage();
	Check(usage.total_heap_bytes > 0, "Resource usage heap bytes invalid");

	auto vmem = MemoryInspector::QueryVirtualAddressSpace();
	Check(!vmem.empty(), "Virtual memory segments empty");

	std::printf("  [OK] Test 4: Developer Inspection Tools\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkCompatibilityFramework() {
	std::printf("\n--- Phase P Benchmarks ---\n");

	// 1. Game Database Lookup Latency Benchmark
	GameDatabase db;
	constexpr int kLookupBatch = 1000000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kLookupBatch; ++i) {
		db.LookupGame("CUSA00001");
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double lookup_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kLookupBatch;
	double lookup_throughput = kLookupBatch / std::chrono::duration<double>(t1 - t0).count();

	std::printf("  [Bench] Title Database Lookup Latency: %.2f ns / lookup (Throughput: %.2f M lookups/sec)\n",
	           lookup_dt_ns, lookup_throughput / 1e6);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase P: Compatibility Framework          \n");
	std::printf("====================================================\n\n");

	TestGameAndCompatibilityDatabase();
	TestPersistentShaderCache();
	TestCrashReporter();
	TestDeveloperTools();

	BenchmarkCompatibilityFramework();

	std::printf("\nCompatibilityFrameworkTests: ALL PASSED\n");
	return 0;
}
