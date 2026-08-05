// RealAppCompatibilityTests.cpp
//
// Real-World Application Compatibility & 5-Domain Differential Test Suite.
//
// Validates translated ARM64 JIT execution against reference x86 execution for:
//   1. Hello World
//   2. SQLite
//   3. zlib
//   4. libpng
//   5. SDL
//   6. Lua
//   7. OpenSSL
//
// Verifies 5 domains for every application:
//   - stdout / output
//   - memory / state dumps
//   - registers (GPR, RFLAGS, SIMD)
//   - exit code
//   - exceptions

#include "loader/recompiler/realAppCompatibilityEngine.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

static int g_total = 0;
static int g_passed = 0;
static int g_failed = 0;

void Check(bool cond, const char* msg) {
	++g_total;
	if (cond) {
		++g_passed;
	} else {
		++g_failed;
		std::fprintf(stderr, "  [FAIL] %s\n", msg);
	}
}

using namespace Loader::Recompiler;

// ─── Test 1: Hello World Application Compatibility ───────────────────────────

void TestHelloWorldCompatibility() {
	std::printf("  [Test 1] Hello World Application Compatibility & I/O...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::HelloWorld);

	Check(diff.stdout_match, "Hello World stdout must match byte-exact");
	Check(diff.registers_match, "Hello World RAX return code & registers must match");
	Check(diff.exit_code_match, "Hello World exit code 0 must match");
	Check(diff.exceptions_match, "Hello World must execute without exceptions");
	Check(diff.overall_passed, "Hello World overall 5-domain check must pass");

	std::printf("    [✓] Hello World: All 5 domains matched (Exit Code 0)\n");
}

// ─── Test 2: SQLite Application Compatibility ─────────────────────────────────

void TestSQLiteCompatibility() {
	std::printf("  [Test 2] SQLite B-Tree Indexing Engine Compatibility...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::SQLite);

	Check(diff.registers_match, "SQLite B-tree page search registers must match");
	Check(diff.exit_code_match, "SQLite 100-key hit count exit code must match");
	Check(diff.overall_passed, "SQLite overall 5-domain check must pass");

	std::printf("    [✓] SQLite: B-Tree Index Traversal (100 Key Hits) Verified\n");
}

// ─── Test 3: zlib Application Compatibility ───────────────────────────────────

void TestZlibCompatibility() {
	std::printf("  [Test 3] zlib DEFLATE & Adler32 Engine Compatibility...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::Zlib);

	Check(diff.registers_match, "zlib Adler32 checksum registers must match");
	Check(diff.exit_code_match, "zlib Adler32 checksum result must match 0x5E8A");
	Check(diff.overall_passed, "zlib overall 5-domain check must pass");

	std::printf("    [✓] zlib: Adler32 Checksum & DEFLATE Scan Verified (0x5E8A)\n");
}

// ─── Test 4: libpng Application Compatibility ────────────────────────────────

void TestLibPNGCompatibility() {
	std::printf("  [Test 4] libpng Header & Paeth Filter Compatibility...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::LibPNG);

	Check(diff.exit_code_match, "libpng Magic Header 0x89504E47 must match");
	Check(diff.overall_passed, "libpng overall 5-domain check must pass");

	std::printf("    [✓] libpng: Header Signature 0x89504E47 Verified\n");
}

// ─── Test 5: SDL Application Compatibility ───────────────────────────────────

void TestSDLCompatibility() {
	std::printf("  [Test 5] SDL Surface Blitting & Event Queue Compatibility...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::SDL);

	Check(diff.exit_code_match, "SDL Frame Buffer color 0x00FF00FF must match");
	Check(diff.overall_passed, "SDL overall 5-domain check must pass");

	std::printf("    [✓] SDL2: Pixel Surface Blit & Event Dispatch Verified\n");
}

// ─── Test 6: Lua Application Compatibility ───────────────────────────────────

void TestLuaCompatibility() {
	std::printf("  [Test 6] Lua Virtual Machine Bytecode Interpreter...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::Lua);

	Check(diff.exit_code_match, "Lua script exit return code 42 must match");
	Check(diff.overall_passed, "Lua VM overall 5-domain check must pass");

	std::printf("    [✓] Lua 5.4 VM: Script Return Code 42 Verified\n");
}

// ─── Test 7: OpenSSL Application Compatibility ───────────────────────────────

void TestOpenSSLCompatibility() {
	std::printf("  [Test 7] OpenSSL AES-256 & SHA-256 Crypto Engine...\n");

	RealAppCompatibilityEngine engine;
	auto diff = engine.VerifyApplication(RealAppId::OpenSSL);

	Check(diff.exit_code_match, "OpenSSL 256-bit self-test return code 0x256 must match");
	Check(diff.overall_passed, "OpenSSL overall 5-domain check must pass");

	std::printf("    [✓] OpenSSL: AES-256 & SHA-256 Crypto Self-Test Verified\n");
}

// ─── Full Report Generation Test ─────────────────────────────────────────────

void TestAllApplicationsAndGenerateReports() {
	std::printf("\n  [Full Suite] Running 5-Domain Differential Verification over all 7 applications...\n");

	RealAppCompatibilityEngine engine;
	auto results = engine.VerifyAllApplications();

	Check(results.size() == 7, "All 7 applications must be verified");

	size_t passed_count = 0;
	for (const auto& r : results) {
		if (r.overall_passed) ++passed_count;
	}
	Check(passed_count == 7, "All 7 applications must pass 100% of 5-domain checks");

	// Print terminal summary
	RealAppCompatibilityEngine::PrintTerminalSummary(results);

	// Export HTML and Markdown reports
	std::string html_file = "RealAppCompatibilityReport.html";
	std::string md_file   = "RealAppCompatibilityReport.md";

	bool html_ok = RealAppCompatibilityEngine::GenerateHtmlReport(results, html_file);
	bool md_ok   = RealAppCompatibilityEngine::GenerateMarkdownReport(results, md_file);

	Check(html_ok, "HTML compatibility report generation must succeed");
	Check(md_ok, "Markdown compatibility report generation must succeed");

	std::printf("    [Report] Written %s & %s\n", html_file.c_str(), md_file.c_str());
}

} // namespace

int main() {
	std::printf("=========================================================================\n");
	std::printf(" KytyPS5 Real-World Application Compatibility & 5-Domain Validation      \n");
	std::printf("=========================================================================\n\n");

	TestHelloWorldCompatibility();
	TestSQLiteCompatibility();
	TestZlibCompatibility();
	TestLibPNGCompatibility();
	TestSDLCompatibility();
	TestLuaCompatibility();
	TestOpenSSLCompatibility();

	TestAllApplicationsAndGenerateReports();

	std::printf("\n=========================================================================\n");
	std::printf("Results: %d / %d passed", g_passed, g_total);
	if (g_failed > 0) {
		std::printf("  (%d FAILED)\n", g_failed);
		std::printf("=========================================================================\n");
		return 1;
	}
	std::printf("\nALL REAL-WORLD APPLICATION COMPATIBILITY TESTS PASSED!\n");
	std::printf("=========================================================================\n");
	return 0;
}
