// Ps5TitleCompatTests.cpp
//
// Automated Unit Tests & Benchmarks for PS5 Title Compatibility Framework

#include "common/logging/log.h"
#include "compat/compatDevTools.h"
#include "compat/compatibilityReporter.h"
#include "compat/titleCompatibility.h"
#include "compat/versionDetection.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace Compat;

static int g_test_count   = 0;
static int g_passed_count = 0;

#define TEST_ASSERT(cond, msg)                                                                     \
	do {                                                                                           \
		g_test_count++;                                                                            \
		if (cond) {                                                                                \
			g_passed_count++;                                                                      \
		} else {                                                                                   \
			std::cerr << "  [FAIL] " << msg << " (" << __FILE__ << ":" << __LINE__ << ")\n";       \
		}                                                                                          \
	} while (0)

static void TestTitleCompatibilityDatabaseSerialization() {
	std::cout << "  [Test 1] Title Database JSON Serialization & Wildcard Lookup...\n";

	TitleCompatibilityDatabase db;

	TitleEntry e1;
	e1.title_id     = "PPSA01234";
	e1.title_name   = "Demon's Souls";
	e1.app_version  = "01.05";
	e1.sdk_version  = "09.00.00";
	e1.status       = GameStatus::Playable;
	e1.notes        = "Runs well with FP16 shader override.";
	e1.kernel_workarounds.relaxed_memory_permissions = true;
	e1.gpu_workarounds.preferred_backend             = PreferredGpuBackend::Metal;

	KnownIssue issue;
	issue.issue_id           = "ISSUE-001";
	issue.category           = IssueCategory::Graphics;
	issue.severity           = IssueSeverity::Medium;
	issue.description        = "Minor shadow flicker on dynamic lights.";
	issue.workaround_applied = "DisablePipelineBarriers";
	issue.status             = IssueStatus::Mitigated;
	e1.known_issues.push_back(issue);

	GameSpecificPatch patch;
	patch.patch_id = "PATCH-60FPS";
	patch.name     = "60 FPS Uncap";
	patch.author   = "KytyTeam";
	patch.writes.push_back({0x1000, {0x90, 0x90}, {0xeb, 0x00}, "Bypass frame rate governor"});
	e1.patches.push_back(patch);

	ShaderOverrideRule rule;
	rule.rule_id       = "SHAD-001";
	rule.shader_hash   = 0x123456789ABCDEF0ULL;
	rule.shader_stage  = "fragment";
	rule.action        = ShaderOverrideAction::ForceFP16;
	e1.shader_overrides.push_back(rule);

	db.AddOrUpdateEntry(e1);

	// Wildcard entry
	TitleEntry e_wildcard;
	e_wildcard.title_id    = "CUSA*";
	e_wildcard.title_name  = "Generic PS4 Backcompat";
	e_wildcard.app_version = "*";
	e_wildcard.status      = GameStatus::Boots;
	db.AddOrUpdateEntry(e_wildcard);

	TEST_ASSERT(db.GetEntryCount() == 2, "Database should contain 2 entries");

	// Serialize & Deserialize
	std::string json_str = db.SaveToJsonString();
	TEST_ASSERT(!json_str.empty(), "JSON serialization should not be empty");

	TitleCompatibilityDatabase db2;
	TEST_ASSERT(db2.LoadFromJsonString(json_str), "JSON string loading should succeed");
	TEST_ASSERT(db2.GetEntryCount() == 2, "Loaded database should contain 2 entries");

	// Exact lookup
	const auto* found = db2.FindEntry("PPSA01234", "01.05");
	TEST_ASSERT(found != nullptr, "Should find exact Title ID match");
	if (found != nullptr) {
		TEST_ASSERT(found->title_name == "Demon's Souls", "Title name should match");
		TEST_ASSERT(found->status == GameStatus::Playable, "Game status should be Playable");
		TEST_ASSERT(found->known_issues.size() == 1, "Should have 1 known issue");
		TEST_ASSERT(found->patches.size() == 1, "Should have 1 patch");
		TEST_ASSERT(found->shader_overrides.size() == 1, "Should have 1 shader override");
		TEST_ASSERT(found->kernel_workarounds.relaxed_memory_permissions, "Relaxed memory flag should be true");
		TEST_ASSERT(found->gpu_workarounds.preferred_backend == PreferredGpuBackend::Metal, "Backend should be Metal");
	}

	// Wildcard lookup
	const auto* found_wc = db2.FindEntry("CUSA00001", "01.00");
	TEST_ASSERT(found_wc != nullptr, "Should match wildcard CUSA*");
	if (found_wc != nullptr) {
		TEST_ASSERT(found_wc->title_name == "Generic PS4 Backcompat", "Wildcard entry match");
	}

	std::cout << "  [OK] Test 1: Title Database JSON Serialization & Wildcard Lookup\n";
}

static void TestVersionDetectionEngine() {
	std::cout << "  [Test 2] Version Detection Engine...\n";

	std::filesystem::path temp_dir = std::filesystem::temp_directory_path() / "kyty_compat_test_app0";
	std::filesystem::create_directories(temp_dir / "sce_sys");

	// Create param.json
	std::ofstream json_file(temp_dir / "sce_sys" / "param.json");
	json_file << R"({
		"titleId": "PPSA09999",
		"titleName": "Test Action Game",
		"appVersion": "02.10",
		"sdkVersion": "09.50.00",
		"contentId": "EP9000-PPSA09999_00-0000000000000000"
	})";
	json_file.close();

	auto info = VersionDetector::DetectFromAppDir(temp_dir);
	TEST_ASSERT(info.valid, "Version detection should be valid");
	TEST_ASSERT(info.title_id == "PPSA09999", "Title ID should match PPSA09999");
	TEST_ASSERT(info.title_name == "Test Action Game", "Title Name should match");
	TEST_ASSERT(info.app_version == "02.10", "App Version should match 02.10");
	TEST_ASSERT(info.sdk_version == "09.50.00", "SDK Version should match 09.50.00");

	std::filesystem::remove_all(temp_dir);
	std::cout << "  [OK] Test 2: Version Detection Engine\n";
}

static void TestAutomatedCompatibilityReporter() {
	std::cout << "  [Test 3] Automated Compatibility Reporter...\n";

	DetectedVersionInfo info;
	info.title_id    = "PPSA07777";
	info.title_name  = "Returnal";
	info.app_version = "01.00";
	info.sdk_version = "09.00.00";
	info.valid       = true;

	CompatibilityReporter reporter;
	reporter.StartSession(info);

	// Simulate rendering 100 frames
	for (int i = 0; i < 100; ++i) {
		reporter.RecordFrame(16.6); // ~60 FPS
	}

	reporter.RecordPatchApplied("60 FPS Uncap");
	reporter.RecordShaderOverrideApplied("ForceFP16_ComputePass");
	reporter.RecordKernelWorkaround("RelaxedMemoryPermissions");
	reporter.RecordGpuWorkaround("ForceDepthFormatConversion");
	reporter.RecordLoadedModule("eboot.bin");
	reporter.RecordLoadedModule("libScePad.sprx");
	reporter.EndSession(GameStatus::Playable);

	auto stats = reporter.GetSessionStats();
	TEST_ASSERT(stats.total_frames == 100, "Total frames recorded should be 100");
	TEST_ASSERT(stats.average_fps >= 59.0 && stats.average_fps <= 61.0, "Average FPS should be ~60 FPS");
	TEST_ASSERT(stats.session_status == GameStatus::Playable, "Session status should be Playable");
	TEST_ASSERT(stats.applied_patches.size() == 1, "Should have 1 patch recorded");

	std::string md_report   = reporter.GenerateMarkdownReportString();
	std::string json_report = reporter.GenerateJsonReportString();

	TEST_ASSERT(md_report.find("# KytyPS5 Compatibility Session Report: Returnal (PPSA07777)") != std::string::npos,
	            "Markdown report title header match");
	TEST_ASSERT(json_report.find("\"title_id\": \"PPSA07777\"") != std::string::npos,
	            "JSON report title_id match");

	std::cout << "  [OK] Test 3: Automated Compatibility Reporter\n";
}

static void TestDeveloperToolsCLI() {
	std::cout << "  [Test 4] Developer Tools & Matrix Exporter...\n";

	std::filesystem::path temp_db = std::filesystem::temp_directory_path() / "kyty_test_compat_db.json";
	std::filesystem::path temp_md = std::filesystem::temp_directory_path() / "kyty_test_matrix.md";

	TitleCompatibilityDatabase db;
	TitleEntry                 entry;
	entry.title_id    = "PPSA00001";
	entry.title_name  = "Astro's Playroom";
	entry.app_version = "01.00";
	entry.status      = GameStatus::Perfect;
	db.AddOrUpdateEntry(entry);
	db.SaveToFile(temp_db);

	// Validate DB
	auto val_res = TitleCompatDevTools::ValidateDatabaseFile(temp_db);
	TEST_ASSERT(val_res.valid, "Database validation should pass");

	// Add Issue
	KnownIssue issue;
	issue.issue_id    = "ISSUE-ASTRO-01";
	issue.description = "Audio pop on startup";
	TEST_ASSERT(TitleCompatDevTools::AddKnownIssue(temp_db, "PPSA00001", issue), "Add issue should succeed");

	// Export Markdown Matrix
	TEST_ASSERT(TitleCompatDevTools::ExportMarkdownMatrix(temp_db, temp_md), "Export matrix should succeed");
	TEST_ASSERT(std::filesystem::exists(temp_md), "Exported markdown file should exist");

	std::filesystem::remove(temp_db);
	std::filesystem::remove(temp_md);
	std::cout << "  [OK] Test 4: Developer Tools & Matrix Exporter\n";
}

static void BenchmarkTitleDatabaseLookup() {
	std::cout << "  [Bench] High-Performance Title Database Lookup...\n";

	TitleCompatibilityDatabase db;
	for (int i = 0; i < 1000; ++i) {
		TitleEntry e;
		char       buf[32];
		std::snprintf(buf, sizeof(buf), "PPSA%05d", i);
		e.title_id    = buf;
		e.app_version = "01.00";
		e.status      = GameStatus::Playable;
		db.AddOrUpdateEntry(e);
	}

	constexpr int NUM_QUERIES = 100000;
	auto          start       = std::chrono::high_resolution_clock::now();

	for (int i = 0; i < NUM_QUERIES; ++i) {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "PPSA%05d", i % 1000);
		const auto* entry = db.FindEntry(buf, "01.00");
		(void)entry;
	}

	auto   end          = std::chrono::high_resolution_clock::now();
	double duration_us  = std::chrono::duration<double, std::micro>(end - start).count();
	double per_query_ns = (duration_us * 1000.0) / static_cast<double>(NUM_QUERIES);
	double throughput   = (static_cast<double>(NUM_QUERIES) / (duration_us / 1000000.0)) / 1000000.0;

	std::printf("  [Bench] 100,000 Title ID Queries across 1,000 DB entries: %.2f ns / query (Throughput: %.2f M queries/sec)\n",
	            per_query_ns, throughput);
	TEST_ASSERT(per_query_ns < 1000.0, "Per query latency should be < 1.0 us");
}

int main() {
	std::cout << "====================================================\n";
	std::cout << " KytyPS5: Title Compatibility Framework Test Suite  \n";
	std::cout << "====================================================\n\n";

	TestTitleCompatibilityDatabaseSerialization();
	TestVersionDetectionEngine();
	TestAutomatedCompatibilityReporter();
	TestDeveloperToolsCLI();
	BenchmarkTitleDatabaseLookup();

	std::cout << "\n====================================================\n";
	std::cout << " Results: " << g_passed_count << "/" << g_test_count << " tests passed\n";
	if (g_passed_count == g_test_count) {
		std::cout << "Ps5TitleCompatTests: ALL PASSED\n\n";
		return 0;
	}

	std::cout << "Ps5TitleCompatTests: SOME TESTS FAILED\n\n";
	return 1;
}
