// CommercialGameValidationTests.cpp
//
// Commercial PS5 Game Subsystem Validation & Multi-Commit Regression Integration Tests.

#include "compat/commercialCompatDashboard.h"
#include "compat/commercialGameValidator.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

using Compat::CommercialCompatDashboard;
using Compat::CommercialGameValidator;
using Compat::CommercialTitleEntry;
using Compat::GameStatus;
using Compat::SubsystemHealth;

#define TEST(name) static void test_##name(); static const bool reg_##name = (test_##name(), true); static void test_##name()
#define ASSERT_TRUE(cond) assert(cond)
#define ASSERT_EQ(a, b) assert((a) == (b))
#define ASSERT_GT(a, b) assert((a) > (b))

TEST(SubsystemValidationSuite12) {
    CommercialGameValidator validator;
    auto report = validator.ValidateGame("CUSA00001", "Demon's Souls Remake");

    ASSERT_EQ(report.title_id, "CUSA00001");
    ASSERT_EQ(report.results.size(), static_cast<size_t>(12));
    ASSERT_TRUE(report.overall_success);
    ASSERT_EQ(report.tests_passed, static_cast<uint32_t>(12));
    ASSERT_EQ(report.tests_failed, static_cast<uint32_t>(0));

    std::printf("  12 Subsystem Commercial Validation Tests: ALL PASSED (%llu ms)\n",
                (unsigned long long)report.total_duration_ms);
}

TEST(CommercialCompatDashboardAndRegressionDiff) {
    // Build Baseline Dashboard (Commit A)
    CommercialCompatDashboard baseline;
    CommercialTitleEntry t1;
    t1.title_id     = "CUSA00001";
    t1.title_name   = "Demon's Souls";
    t1.commit_hash  = "a1b2c3d";
    t1.boot_status  = GameStatus::Boots;
    t1.avg_fps      = 30.0;
    baseline.AddTitleEntry(t1);

    CommercialTitleEntry t2;
    t2.title_id     = "PPSA01234";
    t2.title_name   = "Ratchet & Clank: Rift Apart";
    t2.commit_hash  = "a1b2c3d";
    t2.boot_status  = GameStatus::InGame;
    t2.avg_fps      = 55.0;
    baseline.AddTitleEntry(t2);

    // Build Current Dashboard (Commit B)
    CommercialCompatDashboard current;
    CommercialTitleEntry t1_cur = t1;
    t1_cur.commit_hash = "e5f6g7h";
    t1_cur.boot_status = GameStatus::InGame; // Progression!
    t1_cur.avg_fps     = 45.0;            // FPS Progression!
    current.AddTitleEntry(t1_cur);

    CommercialTitleEntry t2_cur = t2;
    t2_cur.commit_hash = "e5f6g7h";
    t2_cur.boot_status = GameStatus::Boots;  // Regression!
    t2_cur.avg_fps     = 20.0;            // FPS Regression!
    current.AddTitleEntry(t2_cur);

    // Compare
    auto diff = current.CompareAgainst(baseline);

    ASSERT_EQ(diff.baseline_commit, "a1b2c3d");
    ASSERT_EQ(diff.current_commit, "e5f6g7h");
    ASSERT_GT(diff.progressions_count, static_cast<uint32_t>(0));
    ASSERT_GT(diff.regressions_count, static_cast<uint32_t>(0));

    // Export Dashboard & Diff
    const std::filesystem::path tmp_dir = std::filesystem::temp_directory_path() / "kyty_commercial_compat";
    std::filesystem::create_directories(tmp_dir);

    const std::string html_path = (tmp_dir / "dashboard.html").string();
    const std::string md_path   = (tmp_dir / "dashboard.md").string();
    const std::string json_path = (tmp_dir / "dashboard.json").string();

    ASSERT_TRUE(current.ExportHtml(html_path, &diff));
    ASSERT_TRUE(current.ExportMarkdown(md_path, &diff));
    ASSERT_TRUE(current.ExportJson(json_path));

    ASSERT_TRUE(std::filesystem::exists(html_path));
    ASSERT_TRUE(std::filesystem::exists(md_path));
    ASSERT_TRUE(std::filesystem::exists(json_path));

    std::printf("  [OK] Regression Diff Engine: %u Progressions, %u Regressions detected\n",
                diff.progressions_count, diff.regressions_count);
    std::printf("  [OK] Dashboard Exporters: HTML, Markdown, JSON generated successfully\n");

    std::filesystem::remove_all(tmp_dir);
}

int main() {
    std::printf("\n================================================================================\n");
    std::printf("  KytyPS5 — Commercial Game Validation & Regression Test Suite                  \n");
    std::printf("================================================================================\n\n");

    std::printf("All Commercial Game Validation Integration Tests Passed Successfully!\n\n");
    return 0;
}
