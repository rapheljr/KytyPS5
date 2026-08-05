// ps5CompatibilityReport.cpp
//
// PS5 JIT-Enabled Compatibility Report Implementation.

#include "compat/ps5CompatibilityReport.h"

#include "common/logging/log.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace Compat {

// ─── Public API ───────────────────────────────────────────────────────────────

void Ps5CompatibilityReport::SetSessionStats(const ExecutionSessionStats& stats) {
    m_session = stats;
}

void Ps5CompatibilityReport::SetJitSummary(
    const Loader::Recompiler::JitRunSummary& jit) {
    m_jit = jit;
}

bool Ps5CompatibilityReport::Generate(const std::filesystem::path& output_dir,
                                       const std::string& title_id) const {
    std::filesystem::create_directories(output_dir);
    const std::string safe_id = title_id.empty() ? "Unknown" : title_id;
    bool html_ok = SaveHtml(output_dir / ("Ps5CompatibilityReport_" + safe_id + ".html"));
    bool md_ok   = SaveMd  (output_dir / ("Ps5CompatibilityReport_" + safe_id + ".md"));
    return html_ok && md_ok;
}

bool Ps5CompatibilityReport::SaveHtml(const std::filesystem::path& path) const {
    std::ofstream f(path);
    if (!f.is_open()) {
        LOGF("[Ps5CompatibilityReport] Cannot write HTML: %s\n", path.string().c_str());
        return false;
    }
    f << GenerateHtmlString();
    LOGF("[Ps5CompatibilityReport] Written HTML: %s\n", path.string().c_str());
    return true;
}

bool Ps5CompatibilityReport::SaveMd(const std::filesystem::path& path) const {
    std::ofstream f(path);
    if (!f.is_open()) {
        LOGF("[Ps5CompatibilityReport] Cannot write Markdown: %s\n", path.string().c_str());
        return false;
    }
    f << GenerateMdString();
    LOGF("[Ps5CompatibilityReport] Written Markdown: %s\n", path.string().c_str());
    return true;
}

// ─── HTML generation ──────────────────────────────────────────────────────────

std::string Ps5CompatibilityReport::GenerateHtmlString() const {
    std::ostringstream out;
    out << HtmlHeader("KytyPS5 Compatibility Report — " + m_session.title_id);

    out << HtmlSection("Executive Summary",    BuildSummarySection(true));
    out << HtmlSection("JIT Cache Statistics", BuildJitCacheSection(true));
    out << HtmlSection("Frame Telemetry",      BuildFrameSection(true));
    out << HtmlSection("Opcode Distribution",  BuildOpcodeSection(true));
    out << HtmlSection("Subsystem Coverage",   BuildSubsystemSection(true));
    out << HtmlSection("Unsupported Instructions", BuildUnsupportedSection(true));

    out << HtmlFooter();
    return out.str();
}

// ─── Markdown generation ──────────────────────────────────────────────────────

std::string Ps5CompatibilityReport::GenerateMdString() const {
    std::ostringstream out;
    out << "# KytyPS5 Compatibility Report\n\n";
    out << "**Title ID:** " << m_session.title_id << "  \n";
    out << "**Title Name:** " << m_session.title_name << "  \n";
    out << "**App Version:** " << m_session.app_version << "  \n";
    out << "**Generated:** " << m_session.end_timestamp << "\n\n";
    out << "---\n\n";

    out << BuildSummarySection(false);
    out << "\n---\n\n";
    out << BuildJitCacheSection(false);
    out << "\n---\n\n";
    out << BuildFrameSection(false);
    out << "\n---\n\n";
    out << BuildOpcodeSection(false);
    out << "\n---\n\n";
    out << BuildSubsystemSection(false);
    out << "\n---\n\n";
    out << BuildUnsupportedSection(false);

    return out.str();
}

// ─── Content builders ────────────────────────────────────────────────────────

std::string Ps5CompatibilityReport::BuildSummarySection(bool html) const {
    const char* status_str = GameStatusToString(m_session.session_status);
    std::ostringstream out;

    if (html) {
        std::string color = "gray";
        if (m_session.session_status == GameStatus::Perfect)   color = "#00c853";
        else if (m_session.session_status == GameStatus::Playable) color = "#64dd17";
        else if (m_session.session_status == GameStatus::InGame)   color = "#ffd600";
        else if (m_session.session_status == GameStatus::Boots)    color = "#ff6d00";
        else if (m_session.session_status == GameStatus::Nothing)   color = "#d50000";

        out << "<table class='info-table'>";
        out << "<tr><td>Title ID</td><td><b>" << m_session.title_id << "</b></td></tr>";
        out << "<tr><td>Title Name</td><td>" << m_session.title_name << "</td></tr>";
        out << "<tr><td>App Version</td><td>" << m_session.app_version << "</td></tr>";
        out << "<tr><td>SDK Version</td><td>" << m_session.sdk_version << "</td></tr>";
        out << "<tr><td>Status</td><td>" << HtmlBadge(status_str, color) << "</td></tr>";
        out << "<tr><td>Module</td><td>" << m_jit.module_name << "</td></tr>";
        out << "<tr><td>Session Duration</td><td>"
            << std::fixed << std::setprecision(2)
            << m_session.session_duration_sec << " s</td></tr>";
        out << "</table>";

        if (!m_session.fatal_errors.empty()) {
            out << "<div class='alert-error'><b>Fatal Errors</b><ul>";
            for (const auto& e : m_session.fatal_errors) {
                out << "<li>" << e << "</li>";
            }
            out << "</ul></div>";
        }
    } else {
        out << "## Executive Summary\n\n";
        out << "| Field | Value |\n|:---|:---|\n";
        out << "| Title ID | `" << m_session.title_id << "` |\n";
        out << "| Title Name | " << m_session.title_name << " |\n";
        out << "| App Version | " << m_session.app_version << " |\n";
        out << "| SDK Version | " << m_session.sdk_version << " |\n";
        out << "| Status | **" << status_str << "** |\n";
        out << "| Module | " << m_jit.module_name << " |\n";
        out << "| Duration | " << std::fixed << std::setprecision(2)
            << m_session.session_duration_sec << " s |\n\n";
        if (!m_session.fatal_errors.empty()) {
            out << "> **⚠ Fatal Errors**\n";
            for (const auto& e : m_session.fatal_errors)
                out << "> - " << e << "\n";
            out << "\n";
        }
    }
    return out.str();
}

std::string Ps5CompatibilityReport::BuildJitCacheSection(bool html) const {
    std::ostringstream out;
    const auto& j = m_jit;

    auto pct = [](uint64_t num, uint64_t denom) -> double {
        return denom > 0 ? 100.0 * static_cast<double>(num) / static_cast<double>(denom) : 0.0;
    };
    const uint64_t total_disp = j.total_blocks_compiled + j.total_blocks_cache_hit;

    if (html) {
        out << "<table class='info-table'>";
        out << "<tr><td>Blocks Compiled (new)</td><td>" << j.total_blocks_compiled << "</td></tr>";
        out << "<tr><td>Cache Hits</td><td>" << j.total_blocks_cache_hit
            << " <small>(" << std::fixed << std::setprecision(1) << j.cache_hit_rate_pct << "%)</small></td></tr>";
        out << "<tr><td>Cache Evictions</td><td>" << j.total_blocks_evicted
            << " <small>(" << std::fixed << std::setprecision(1) << j.cache_evict_rate_pct << "%)</small></td></tr>";
        out << "<tr><td>IR Instructions</td><td>" << j.total_ir_instructions << "</td></tr>";
        out << "<tr><td>ARM64 Instructions</td><td>" << j.total_arm64_instructions << "</td></tr>";
        out << "<tr><td>JIT Cycles (est.)</td><td>" << j.total_jit_cycles << "</td></tr>";
        out << "</table>";
        out << "<p>Cache hit rate: " << HtmlProgressBar(j.cache_hit_rate_pct, "#00c853") << "</p>";
    } else {
        out << "## JIT Cache Statistics\n\n";
        out << "| Metric | Value |\n|:---|---:|\n";
        out << "| Blocks Compiled | " << j.total_blocks_compiled << " |\n";
        out << "| Cache Hits | " << j.total_blocks_cache_hit
            << " (" << std::fixed << std::setprecision(1) << j.cache_hit_rate_pct << "%) |\n";
        out << "| Cache Evictions | " << j.total_blocks_evicted
            << " (" << std::fixed << std::setprecision(1) << j.cache_evict_rate_pct << "%) |\n";
        out << "| IR Instructions | " << j.total_ir_instructions << " |\n";
        out << "| ARM64 Instructions | " << j.total_arm64_instructions << " |\n";
        out << "| JIT Cycles (est.) | " << j.total_jit_cycles << " |\n\n";
    }
    return out.str();
}

std::string Ps5CompatibilityReport::BuildFrameSection(bool html) const {
    std::ostringstream out;
    const auto& j = m_jit;

    if (html) {
        out << "<table class='info-table'>";
        out << "<tr><td>Total Frames</td><td>" << j.total_frames << "</td></tr>";
        out << "<tr><td>Avg FPS</td><td>" << std::fixed << std::setprecision(1)
            << j.avg_fps << "</td></tr>";
        out << "<tr><td>Avg Frame Time</td><td>" << std::fixed << std::setprecision(2)
            << j.avg_frame_time_ms << " ms</td></tr>";
        out << "<tr><td>Min Frame Time</td><td>" << std::fixed << std::setprecision(2)
            << j.min_frame_time_ms << " ms</td></tr>";
        out << "<tr><td>Max Frame Time</td><td>" << std::fixed << std::setprecision(2)
            << j.max_frame_time_ms << " ms</td></tr>";
        out << "<tr><td>Syscalls Dispatched</td><td>" << j.total_syscalls << "</td></tr>";
        out << "<tr><td>PM4 Packets</td><td>" << j.total_pm4_packets << "</td></tr>";
        out << "</table>";
    } else {
        out << "## Frame Telemetry\n\n";
        out << "| Metric | Value |\n|:---|---:|\n";
        out << "| Total Frames | " << j.total_frames << " |\n";
        out << "| Avg FPS | " << std::fixed << std::setprecision(1) << j.avg_fps << " |\n";
        out << "| Avg Frame Time | " << std::fixed << std::setprecision(2) << j.avg_frame_time_ms << " ms |\n";
        out << "| Min Frame Time | " << std::fixed << std::setprecision(2) << j.min_frame_time_ms << " ms |\n";
        out << "| Max Frame Time | " << std::fixed << std::setprecision(2) << j.max_frame_time_ms << " ms |\n";
        out << "| Syscalls | " << j.total_syscalls << " |\n";
        out << "| PM4 Packets | " << j.total_pm4_packets << " |\n\n";
    }
    return out.str();
}

std::string Ps5CompatibilityReport::BuildOpcodeSection(bool html) const {
    std::ostringstream out;
    const auto& top = m_jit.top_opcodes;

    if (html) {
        if (top.empty()) {
            out << "<p><i>No opcode data collected.</i></p>";
        } else {
            out << "<table class='data-table'><thead><tr>"
                << "<th>Rank</th><th>Opcode</th><th>Dispatch Count</th><th>Distribution</th>"
                << "</tr></thead><tbody>";
            const uint64_t max_cnt = top[0].second;
            for (size_t i = 0; i < top.size(); ++i) {
                const double pct = max_cnt > 0
                    ? 100.0 * static_cast<double>(top[i].second) / static_cast<double>(max_cnt)
                    : 0.0;
                out << "<tr><td>" << (i + 1) << "</td>"
                    << "<td><code>0x" << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(top[i].first) << std::dec << "</code></td>"
                    << "<td>" << top[i].second << "</td>"
                    << "<td>" << HtmlProgressBar(pct, "#1565c0") << "</td></tr>";
            }
            out << "</tbody></table>";
        }
    } else {
        out << "## Opcode Distribution (Top 20)\n\n";
        if (top.empty()) {
            out << "*No opcode data collected.*\n\n";
        } else {
            out << "| Rank | Opcode | Count |\n|:---:|:---:|---:|\n";
            for (size_t i = 0; i < top.size(); ++i) {
                char hex[8];
                std::snprintf(hex, sizeof(hex), "0x%02X", top[i].first);
                out << "| " << (i + 1) << " | `" << hex << "` | "
                    << top[i].second << " |\n";
            }
            out << "\n";
        }
    }
    return out.str();
}

std::string Ps5CompatibilityReport::BuildSubsystemSection(bool html) const {
    std::ostringstream out;
    const auto& calls = m_jit.subsystem_calls;

    if (html) {
        if (calls.empty()) {
            out << "<p><i>No subsystem calls recorded.</i></p>";
        } else {
            out << "<table class='data-table'><thead><tr>"
                << "<th>Stub Name</th><th>Call Count</th>"
                << "</tr></thead><tbody>";
            for (const auto& [name, cnt] : calls) {
                out << "<tr><td><code>" << name << "</code></td><td>" << cnt << "</td></tr>";
            }
            out << "</tbody></table>";
        }
    } else {
        out << "## Subsystem Coverage\n\n";
        if (calls.empty()) {
            out << "*No subsystem calls recorded.*\n\n";
        } else {
            out << "| Stub Name | Call Count |\n|:---|---:|\n";
            for (const auto& [name, cnt] : calls) {
                out << "| `" << name << "` | " << cnt << " |\n";
            }
            out << "\n";
        }
    }
    return out.str();
}

std::string Ps5CompatibilityReport::BuildUnsupportedSection(bool html) const {
    std::ostringstream out;
    const auto& unsup = m_jit.unsupported_instructions;

    if (html) {
        if (unsup.empty()) {
            out << "<p style='color:#00c853'><b>✔ No unsupported instructions detected.</b></p>";
        } else {
            out << "<div class='alert-warning'><b>⚠ Unsupported Instructions Detected</b></div>";
            out << "<table class='data-table'><thead><tr>"
                << "<th>Encoding</th><th>Frequency</th>"
                << "</tr></thead><tbody>";
            for (const auto& [enc, cnt] : unsup) {
                out << "<tr><td><code>" << enc << "</code></td><td>" << cnt << "</td></tr>";
            }
            out << "</tbody></table>";
        }
    } else {
        out << "## Unsupported Instructions\n\n";
        if (unsup.empty()) {
            out << "✅ **No unsupported instructions detected.**\n\n";
        } else {
            out << "> ⚠️ **Warning**: The following instructions were not emulated.\n\n";
            out << "| Encoding | Frequency |\n|:---|---:|\n";
            for (const auto& [enc, cnt] : unsup) {
                out << "| `" << enc << "` | " << cnt << " |\n";
            }
            out << "\n";
        }
    }
    return out.str();
}

// ─── HTML helpers ─────────────────────────────────────────────────────────────

std::string Ps5CompatibilityReport::HtmlHeader(const std::string& title) {
    return R"(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>)" + title + R"(</title>
<style>
  :root { --bg: #0d1117; --surface: #161b22; --border: #30363d;
          --text: #c9d1d9; --accent: #58a6ff; --green: #00c853; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background: var(--bg); color: var(--text);
         font-family: 'Segoe UI', system-ui, sans-serif;
         font-size: 14px; line-height: 1.6; padding: 2rem; }
  h1 { color: var(--accent); font-size: 1.8rem; margin-bottom: 2rem; }
  .section { background: var(--surface); border: 1px solid var(--border);
             border-radius: 8px; margin-bottom: 1.5rem; overflow: hidden; }
  .section-title { background: #1c2128; padding: 0.75rem 1.25rem;
                   font-size: 1rem; font-weight: 600; color: var(--accent);
                   border-bottom: 1px solid var(--border); }
  .section-body { padding: 1.25rem; }
  .info-table { width: 100%; border-collapse: collapse; }
  .info-table td { padding: 0.4rem 0.75rem; border-bottom: 1px solid var(--border); }
  .info-table td:first-child { color: #8b949e; width: 40%; }
  .data-table { width: 100%; border-collapse: collapse; }
  .data-table th { padding: 0.5rem 0.75rem; background: #1c2128;
                   text-align: left; font-size: 0.85rem; color: #8b949e; }
  .data-table td { padding: 0.45rem 0.75rem; border-bottom: 1px solid var(--border); }
  code { background: #1c2128; padding: 0.1rem 0.35rem; border-radius: 4px;
         font-size: 0.85rem; font-family: 'Cascadia Code', 'Fira Code', monospace; }
  .badge { display: inline-block; padding: 0.2rem 0.7rem; border-radius: 12px;
           font-size: 0.85rem; font-weight: 600; color: #fff; }
  .progress-bar-wrap { background: var(--border); border-radius: 4px;
                       height: 10px; width: 200px; display: inline-block; }
  .progress-bar-fill { height: 100%; border-radius: 4px; }
  .alert-error { background: rgba(213,0,0,0.1); border: 1px solid #d50000;
                 padding: 0.75rem 1rem; border-radius: 6px; margin-bottom: 1rem; }
  .alert-warning { background: rgba(255,109,0,0.1); border: 1px solid #ff6d00;
                   padding: 0.75rem 1rem; border-radius: 6px; margin-bottom: 1rem; }
</style>
</head>
<body>
<h1>🎮 KytyPS5 Compatibility Report</h1>
)";
}

std::string Ps5CompatibilityReport::HtmlFooter() {
    return R"(
<footer style="margin-top:3rem;color:#8b949e;font-size:0.8rem;text-align:center;">
  Generated by KytyPS5 ARM64 JIT Compatibility Engine
</footer>
</body>
</html>
)";
}

std::string Ps5CompatibilityReport::HtmlSection(const std::string& heading,
                                                  const std::string& body) {
    return "<div class='section'>"
           "<div class='section-title'>" + heading + "</div>"
           "<div class='section-body'>" + body + "</div>"
           "</div>\n";
}

std::string Ps5CompatibilityReport::HtmlTable(const std::string& headers_row,
                                               const std::vector<std::string>& rows) {
    std::string out = "<table class='data-table'><thead><tr>" + headers_row +
                      "</tr></thead><tbody>";
    for (const auto& r : rows) out += "<tr>" + r + "</tr>";
    out += "</tbody></table>";
    return out;
}

std::string Ps5CompatibilityReport::HtmlBadge(const std::string& text,
                                               const std::string& color) {
    return "<span class='badge' style='background:" + color + "'>" + text + "</span>";
}

std::string Ps5CompatibilityReport::HtmlProgressBar(double pct, const std::string& color) {
    const double clamped = std::max(0.0, std::min(100.0, pct));
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "<span class='progress-bar-wrap'>"
                  "<span class='progress-bar-fill' style='width:%.1f%%;background:%s'></span>"
                  "</span> <small>%.1f%%</small>",
                  clamped, color.c_str(), clamped);
    return buf;
}

} // namespace Compat
