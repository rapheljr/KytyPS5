// ps5CompatibilityReport.h
//
// PS5 JIT-Enabled Compatibility Report Generator.
//
// Extends CompatibilityReporter with JIT-specific sections:
//   - JIT Cache Statistics (hit rate, eviction rate, cache utilisation)
//   - Opcode Distribution (top-20 hottest x86 opcodes by dispatch count)
//   - Unsupported Instructions (list with frequency counts, auto-reported)
//   - Subsystem Coverage (which stubs were called and how many times)
//   - Frame Telemetry (min/max/avg frame time, frame count, estimated FPS)
//   - CPU / GPU Usage (JIT cycle counts, PM4 packet counts)
//
// Output formats: self-contained HTML + Markdown.
// Written automatically at emulator shutdown.

#ifndef COMPAT_PS5_COMPATIBILITY_REPORT_H
#define COMPAT_PS5_COMPATIBILITY_REPORT_H

#include "common/common.h"
#include "compat/compatibilityReporter.h"
#include "loader/recompiler/jitTelemetryCollector.h"

#include <filesystem>
#include <string>

namespace Compat {

class Ps5CompatibilityReport {
public:
    Ps5CompatibilityReport() = default;
    ~Ps5CompatibilityReport() = default;

    KYTY_CLASS_NO_COPY(Ps5CompatibilityReport);

    /// Attach base compatibility data and JIT telemetry before generating.
    void SetSessionStats(const ExecutionSessionStats& stats);
    void SetJitSummary(const Loader::Recompiler::JitRunSummary& jit);

    /// Generate both report files.
    /// Returns true if both files were written successfully.
    bool Generate(const std::filesystem::path& output_dir,
                  const std::string& title_id) const;

    /// Generate HTML report string (for embedding or testing).
    [[nodiscard]] std::string GenerateHtmlString() const;

    /// Generate Markdown report string.
    [[nodiscard]] std::string GenerateMdString() const;

    /// Save HTML to @p path.
    [[nodiscard]] bool SaveHtml(const std::filesystem::path& path) const;

    /// Save Markdown to @p path.
    [[nodiscard]] bool SaveMd(const std::filesystem::path& path) const;

private:
    ExecutionSessionStats               m_session;
    Loader::Recompiler::JitRunSummary   m_jit;

    // HTML helpers
    static std::string HtmlHeader(const std::string& title);
    static std::string HtmlFooter();
    static std::string HtmlSection(const std::string& heading, const std::string& body);
    static std::string HtmlTable(const std::string& headers_row,
                                  const std::vector<std::string>& rows);
    static std::string HtmlBadge(const std::string& text, const std::string& color);
    static std::string HtmlProgressBar(double pct, const std::string& color);

    // Content builders
    std::string BuildSummarySection(bool html) const;
    std::string BuildJitCacheSection(bool html) const;
    std::string BuildOpcodeSection(bool html) const;
    std::string BuildUnsupportedSection(bool html) const;
    std::string BuildSubsystemSection(bool html) const;
    std::string BuildFrameSection(bool html) const;
};

} // namespace Compat

#endif // COMPAT_PS5_COMPATIBILITY_REPORT_H
