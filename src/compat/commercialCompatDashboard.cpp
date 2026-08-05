// commercialCompatDashboard.cpp
//
// Commercial PS5 Game Compatibility Dashboard & Multi-Commit Regression Diff Engine Implementation.

#include "compat/commercialCompatDashboard.h"

#include "common/logging/log.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace Compat {

const char* SubsystemHealthToString(SubsystemHealth health) {
    switch (health) {
        case SubsystemHealth::OK:      return "OK";
        case SubsystemHealth::Warning: return "Warning";
        case SubsystemHealth::Error:   return "Error";
        case SubsystemHealth::Crash:   return "Crash";
        case SubsystemHealth::Missing: return "Missing";
    }
    return "Unknown";
}

void CommercialCompatDashboard::AddTitleEntry(const CommercialTitleEntry& entry) {
    m_entries.push_back(entry);
}

RegressionDiffReport CommercialCompatDashboard::CompareAgainst(const CommercialCompatDashboard& baseline) const {
    RegressionDiffReport report;
    report.total_titles = static_cast<uint32_t>(m_entries.size());

    std::unordered_map<std::string, CommercialTitleEntry> base_map;
    for (const auto& e : baseline.GetEntries()) {
        base_map[e.title_id] = e;
        report.baseline_commit = e.commit_hash;
    }

    for (const auto& cur : m_entries) {
        if (report.current_commit.empty()) {
            report.current_commit = cur.commit_hash;
        }

        auto it = base_map.find(cur.title_id);
        if (it == base_map.end()) {
            RegressionItem item;
            item.title_id     = cur.title_id;
            item.title_name   = cur.title_name;
            item.change_type  = "New Title Added";
            item.description  = "Newly tested title: status " + std::string(GameStatusToString(cur.boot_status));
            item.baseline_val = "N/A";
            item.current_val  = GameStatusToString(cur.boot_status);
            report.items.push_back(item);
            continue;
        }

        const auto& base = it->second;

        // Check status progression / regression
        int base_rank = static_cast<int>(base.boot_status);
        int cur_rank  = static_cast<int>(cur.boot_status);

        if (cur_rank > base_rank) {
            report.progressions_count++;
            RegressionItem item;
            item.title_id     = cur.title_id;
            item.title_name   = cur.title_name;
            item.change_type  = "Progression";
            item.description  = "Status upgraded from " + std::string(GameStatusToString(base.boot_status)) +
                                " to " + std::string(GameStatusToString(cur.boot_status));
            item.baseline_val = GameStatusToString(base.boot_status);
            item.current_val  = GameStatusToString(cur.boot_status);
            report.items.push_back(item);
        } else if (cur_rank < base_rank) {
            report.regressions_count++;
            RegressionItem item;
            item.title_id     = cur.title_id;
            item.title_name   = cur.title_name;
            item.change_type  = "Regression";
            item.description  = "Status downgraded from " + std::string(GameStatusToString(base.boot_status)) +
                                " to " + std::string(GameStatusToString(cur.boot_status));
            item.baseline_val = GameStatusToString(base.boot_status);
            item.current_val  = GameStatusToString(cur.boot_status);
            report.items.push_back(item);
        }

        // Check FPS performance delta (>5% change)
        if (base.avg_fps > 0.0) {
            double fps_delta_pct = ((cur.avg_fps - base.avg_fps) / base.avg_fps) * 100.0;
            if (fps_delta_pct > 5.0) {
                report.progressions_count++;
                RegressionItem item;
                item.title_id     = cur.title_id;
                item.title_name   = cur.title_name;
                item.change_type  = "Performance Progression";
                item.description  = "Framerate increased by +" + std::to_string(static_cast<int>(fps_delta_pct)) + "%";
                item.baseline_val = std::to_string(static_cast<int>(base.avg_fps)) + " FPS";
                item.current_val  = std::to_string(static_cast<int>(cur.avg_fps)) + " FPS";
                report.items.push_back(item);
            } else if (fps_delta_pct < -5.0) {
                report.regressions_count++;
                RegressionItem item;
                item.title_id     = cur.title_id;
                item.title_name   = cur.title_name;
                item.change_type  = "Performance Regression";
                item.description  = "Framerate dropped by " + std::to_string(static_cast<int>(fps_delta_pct)) + "%";
                item.baseline_val = std::to_string(static_cast<int>(base.avg_fps)) + " FPS";
                item.current_val  = std::to_string(static_cast<int>(cur.avg_fps)) + " FPS";
                report.items.push_back(item);
            }
        }
    }

    return report;
}

bool CommercialCompatDashboard::ExportHtml(const std::string& filepath, const RegressionDiffReport* diff) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n"
      << "<meta charset=\"UTF-8\">\n<title>KytyPS5 Commercial Compatibility Dashboard</title>\n"
      << "<style>\n"
      << "  body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0c0e14; color: #d1d5db; margin: 0; padding: 24px; }\n"
      << "  h1, h2 { color: #38bdf8; }\n"
      << "  .card-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 16px; margin-bottom: 24px; }\n"
      << "  .card { background: #1e293b; padding: 16px; border-radius: 8px; border: 1px solid #334155; }\n"
      << "  .card-val { font-size: 26px; font-weight: bold; color: #38bdf8; margin-top: 8px; }\n"
      << "  table { width: 100%; border-collapse: collapse; margin-top: 12px; background: #1e293b; border-radius: 8px; overflow: hidden; }\n"
      << "  th, td { padding: 12px; text-align: left; border-bottom: 1px solid #0c0e14; font-size: 14px; }\n"
      << "  th { background: #0284c7; color: #ffffff; }\n"
      << "  .badge { padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 12px; display: inline-block; }\n"
      << "  .badge-playable { background: #16a34a; color: #ffffff; }\n"
      << "  .badge-ingame   { background: #65a30d; color: #ffffff; }\n"
      << "  .badge-boots    { background: #d97706; color: #ffffff; }\n"
      << "  .badge-crash    { background: #dc2626; color: #ffffff; }\n"
      << "  .badge-ok       { background: #22c55e; color: #ffffff; }\n"
      << "  .badge-err      { background: #ef4444; color: #ffffff; }\n"
      << "  .diff-progression { background: #064e3b; border-left: 4px solid #10b981; padding: 12px; margin-bottom: 8px; border-radius: 4px; }\n"
      << "  .diff-regression  { background: #7f1d1d; border-left: 4px solid #f43f5e; padding: 12px; margin-bottom: 8px; border-radius: 4px; }\n"
      << "</style>\n</head>\n<body>\n";

    f << "<h1>KytyPS5 Commercial Game Compatibility Dashboard</h1>\n"
      << "<p>Automated validation & multi-commit regression tracking across commercial PS5 titles.</p>\n";

    // Overall metrics summary cards
    uint32_t playable_cnt = 0, ingame_cnt = 0, boots_cnt = 0, crash_cnt = 0;
    for (const auto& e : m_entries) {
        if (e.boot_status == GameStatus::Playable || e.boot_status == GameStatus::Perfect) playable_cnt++;
        else if (e.boot_status == GameStatus::InGame) ingame_cnt++;
        else if (e.boot_status == GameStatus::Boots || e.boot_status == GameStatus::Intro) boots_cnt++;
        else crash_cnt++;
    }

    f << "<div class=\"card-grid\">\n"
      << "  <div class=\"card\"><div>Total Titles</div><div class=\"card-val\">" << m_entries.size() << "</div></div>\n"
      << "  <div class=\"card\"><div>Playable</div><div class=\"card-val\" style=\"color:#22c55e;\">" << playable_cnt << "</div></div>\n"
      << "  <div class=\"card\"><div>In-Game</div><div class=\"card-val\" style=\"color:#84cc16;\">" << ingame_cnt << "</div></div>\n"
      << "  <div class=\"card\"><div>Boots</div><div class=\"card-val\" style=\"color:#eab308;\">" << boots_cnt << "</div></div>\n"
      << "  <div class=\"card\"><div>Crashes</div><div class=\"card-val\" style=\"color:#ef4444;\">" << crash_cnt << "</div></div>\n"
      << "</div>\n";

    // Multi-commit regression diff section if provided
    if (diff != nullptr && !diff->items.empty()) {
        f << "<h2>Multi-Commit Regression Diff Report (" << diff->baseline_commit << " &rarr; " << diff->current_commit << ")</h2>\n";
        for (const auto& item : diff->items) {
            bool is_progression = (item.change_type.find("Progression") != std::string::npos);
            const char* css_cls = is_progression ? "diff-progression" : "diff-regression";
            f << "<div class=\"" << css_cls << "\">\n"
              << "  <strong>[" << item.change_type << "] " << item.title_name << " (" << item.title_id << ")</strong><br>"
              << "  " << item.description << " (Baseline: " << item.baseline_val << " &rarr; Current: " << item.current_val << ")\n"
              << "</div>\n";
        }
    }

    // 7-Subsystem Compatibility Table
    f << "<h2>Subsystem Compatibility Matrix</h2>\n<table>\n"
      << "<tr><th>Title ID</th><th>Game Name</th><th>Boot</th><th>CPU</th><th>GPU</th><th>Kernel</th><th>Audio</th><th>Input</th><th>FS</th><th>FPS</th></tr>\n";

    for (const auto& e : m_entries) {
        const char* boot_cls = "badge-boots";
        if (e.boot_status == GameStatus::Playable || e.boot_status == GameStatus::Perfect) boot_cls = "badge-playable";
        else if (e.boot_status == GameStatus::InGame) boot_cls = "badge-ingame";
        else if (e.boot_status == GameStatus::Nothing || e.crash_count > 0) boot_cls = "badge-crash";

        f << "<tr>"
          << "<td><code>" << e.title_id << "</code></td>"
          << "<td>" << e.title_name << "</td>"
          << "<td><span class=\"badge " << boot_cls << "\">" << GameStatusToString(e.boot_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.cpu_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.cpu_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.gpu_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.gpu_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.kernel_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.kernel_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.audio_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.audio_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.input_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.input_status) << "</span></td>"
          << "<td><span class=\"badge " << (e.filesystem_status == SubsystemHealth::OK ? "badge-ok" : "badge-err") << "\">" << SubsystemHealthToString(e.filesystem_status) << "</span></td>"
          << "<td>" << std::fixed << std::setprecision(1) << e.avg_fps << "</td>"
          << "</tr>\n";
    }

    f << "</table>\n</body>\n</html>\n";
    return true;
}

bool CommercialCompatDashboard::ExportMarkdown(const std::string& filepath, const RegressionDiffReport* diff) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "# KytyPS5 Commercial Compatibility Dashboard\n\n";
    f << "| Title ID | Game Name | Boot | CPU | GPU | Kernel | Audio | Input | FS | FPS |\n";
    f << "|:---|:---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|\n";

    for (const auto& e : m_entries) {
        f << "| `" << e.title_id << "` | " << e.title_name << " | `"
          << GameStatusToString(e.boot_status) << "` | "
          << SubsystemHealthToString(e.cpu_status) << " | "
          << SubsystemHealthToString(e.gpu_status) << " | "
          << SubsystemHealthToString(e.kernel_status) << " | "
          << SubsystemHealthToString(e.audio_status) << " | "
          << SubsystemHealthToString(e.input_status) << " | "
          << SubsystemHealthToString(e.filesystem_status) << " | "
          << std::fixed << std::setprecision(1) << e.avg_fps << " |\n";
    }

    if (diff != nullptr && !diff->items.empty()) {
        f << "\n## Multi-Commit Regression Diff Report (" << diff->baseline_commit << " -> " << diff->current_commit << ")\n\n";
        for (const auto& item : diff->items) {
            f << "- **[" << item.change_type << "]** `" << item.title_id << "`: "
              << item.description << " (" << item.baseline_val << " -> " << item.current_val << ")\n";
        }
    }

    return true;
}

bool CommercialCompatDashboard::ExportJson(const std::string& filepath) const {
    std::ofstream f(filepath);
    if (!f.is_open()) return false;

    f << "{\n  \"titles\": [\n";
    for (size_t i = 0; i < m_entries.size(); ++i) {
        const auto& e = m_entries[i];
        f << "    {\n"
          << "      \"title_id\": \"" << e.title_id << "\",\n"
          << "      \"title_name\": \"" << e.title_name << "\",\n"
          << "      \"commit_hash\": \"" << e.commit_hash << "\",\n"
          << "      \"boot_status\": \"" << GameStatusToString(e.boot_status) << "\",\n"
          << "      \"cpu_status\": \"" << SubsystemHealthToString(e.cpu_status) << "\",\n"
          << "      \"gpu_status\": \"" << SubsystemHealthToString(e.gpu_status) << "\",\n"
          << "      \"kernel_status\": \"" << SubsystemHealthToString(e.kernel_status) << "\",\n"
          << "      \"avg_fps\": " << e.avg_fps << ",\n"
          << "      \"crash_count\": " << e.crash_count << "\n"
          << "    }" << (i + 1 < m_entries.size() ? "," : "") << "\n";
    }
    f << "  ]\n}\n";
    return true;
}

} // namespace Compat
