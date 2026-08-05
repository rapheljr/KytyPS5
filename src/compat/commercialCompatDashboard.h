// commercialCompatDashboard.h
//
// Commercial PS5 Game Compatibility Dashboard & Multi-Commit Regression Diff Engine.
//
// Tracks 7 Subsystem Health Badges per Game:
//   1. Boot Status (Playable / InGame / Intro / Boots / Nothing / Crash)
//   2. CPU Status (OK / Stall / Fault)
//   3. GPU Status (OK / Fallback / Error)
//   4. Kernel Status (OK / Syscall Missing)
//   5. Audio Status (OK / Muted / Error)
//   6. Input Status (OK / Disconnected)
//   7. Filesystem Status (OK / File Not Found)
//
// Additional Metrics:
//   - Frame Rate (Avg / Min / Max FPS)
//   - Crashes & Stack Traces
//   - Unsupported Syscalls & Opcodes
//   - Multi-Commit Regression Analysis (Progressions, Regressions, Performance Deltas)

#ifndef COMPAT_COMMERCIAL_COMPAT_DASHBOARD_H
#define COMPAT_COMMERCIAL_COMPAT_DASHBOARD_H

#include "common/common.h"
#include "compat/titleCompatibility.h"

#include <chrono>
#include <map>
#include <string>
#include <vector>

namespace Compat {

enum class SubsystemHealth {
    OK = 0,
    Warning,
    Error,
    Crash,
    Missing
};

const char* SubsystemHealthToString(SubsystemHealth health);

struct CommercialTitleEntry {
    std::string title_id;
    std::string title_name;
    std::string commit_hash;
    std::string build_id;

    GameStatus      boot_status       = GameStatus::Unknown;
    SubsystemHealth cpu_status        = SubsystemHealth::OK;
    SubsystemHealth gpu_status        = SubsystemHealth::OK;
    SubsystemHealth kernel_status     = SubsystemHealth::OK;
    SubsystemHealth audio_status      = SubsystemHealth::OK;
    SubsystemHealth input_status      = SubsystemHealth::OK;
    SubsystemHealth filesystem_status = SubsystemHealth::OK;

    double avg_fps         = 0.0;
    double min_fps         = 0.0;
    double max_fps         = 0.0;
    uint32_t crash_count   = 0;
    std::string last_crash_trace;

    std::vector<std::string> unsupported_syscalls;
    std::vector<std::string> unsupported_opcodes;
};

// ─── Multi-Commit Regression Diff Report ─────────────────────────────────────

struct RegressionItem {
    std::string title_id;
    std::string title_name;
    std::string change_type; // "Progression", "Regression", "Performance Delta"
    std::string description;
    std::string baseline_val;
    std::string current_val;
};

struct RegressionDiffReport {
    std::string baseline_commit;
    std::string current_commit;

    uint32_t progressions_count = 0;
    uint32_t regressions_count  = 0;
    uint32_t total_titles        = 0;

    std::vector<RegressionItem> items;
};

// ─── Dashboard Class ──────────────────────────────────────────────────────────

class CommercialCompatDashboard {
public:
    CommercialCompatDashboard() = default;
    ~CommercialCompatDashboard() = default;

    KYTY_CLASS_NO_COPY(CommercialCompatDashboard);

    void AddTitleEntry(const CommercialTitleEntry& entry);
    [[nodiscard]] const std::vector<CommercialTitleEntry>& GetEntries() const noexcept { return m_entries; }

    /// Compare current dashboard against @p baseline dashboard.
    [[nodiscard]] RegressionDiffReport CompareAgainst(const CommercialCompatDashboard& baseline) const;

    /// Multi-format Exporters
    bool ExportHtml(const std::string& filepath, const RegressionDiffReport* diff = nullptr) const;
    bool ExportMarkdown(const std::string& filepath, const RegressionDiffReport* diff = nullptr) const;
    bool ExportJson(const std::string& filepath) const;

private:
    std::vector<CommercialTitleEntry> m_entries;
};

} // namespace Compat

#endif // COMPAT_COMMERCIAL_COMPAT_DASHBOARD_H
