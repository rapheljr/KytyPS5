// OpcodeCoverageFrameworkTests.cpp
//
// Complete Test Suite for ARM64 JIT Opcode Coverage Framework & Test Variant Generator.

#include "loader/recompiler/opcodeCoverageFramework.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "OpcodeCoverageFrameworkTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestOpcodeInventoryAndJsonExport() {
	std::printf("  [Coverage Test 1] Testing Opcode Inventory Build & JSON/Markdown Export...\n");

	auto inventory = OpcodeInventory::BuildInventory();
	Check(inventory.size() == 512, "Inventory must contain 512 primary & 2-byte opcodes");

	bool json_ok = OpcodeInventory::ExportJson(inventory, "OpcodeCoverage.json");
	Check(json_ok, "Exporting OpcodeCoverage.json must succeed");

	bool md_ok = OpcodeInventory::ExportMarkdown(inventory, "OpcodeCoverage.md");
	Check(md_ok, "Exporting OpcodeCoverage.md must succeed");

	Check(std::filesystem::exists("OpcodeCoverage.json"), "OpcodeCoverage.json must exist on disk");
	Check(std::filesystem::exists("OpcodeCoverage.md"), "OpcodeCoverage.md must exist on disk");

	std::printf("  [OK] Coverage Test 1: Inventory & Export passed\n");
}

void TestAutoTestVariantGenerator() {
	std::printf("  [Coverage Test 2] Testing Automatic Test Variant Generation...\n");

	OpcodeMetadata meta;
	meta.mnemonic = "ADD/SUB/INC/DEC/NEG";
	meta.category = OpcodeCategory::Arithmetic;

	auto variants = AutoTestGenerator::GenerateVariantsForOpcode(meta);
	Check(!variants.empty(), "AutoTestGenerator must generate instruction variants");
	Check(variants[0].back() == 0xC3, "Instruction variant must terminate with RET");

	std::printf("  [OK] Coverage Test 2: Auto Variant Generator passed\n");
}

void TestCoverageHeatmapAndDashboardGenerator() {
	std::printf("  [Coverage Test 3] Testing HTML Heatmap, Dashboard & CSV History Generator...\n");

	auto inventory = OpcodeInventory::BuildInventory();

	bool html_ok = CoverageHeatmapGenerator::GenerateHtmlHeatmap(inventory, "CoverageHeatmap.html");
	Check(html_ok, "Exporting CoverageHeatmap.html must succeed");

	bool dash_ok = CoverageHeatmapGenerator::GenerateDashboardMarkdown(inventory, "CoverageDashboard.md");
	Check(dash_ok, "Exporting CoverageDashboard.md must succeed");

	bool csv_ok = CoverageHeatmapGenerator::AppendHistoryCsv(inventory, "CoverageHistory.csv");
	Check(csv_ok, "Appending CoverageHistory.csv must succeed");

	Check(std::filesystem::exists("CoverageHeatmap.html"), "CoverageHeatmap.html must exist");
	Check(std::filesystem::exists("CoverageDashboard.md"), "CoverageDashboard.md must exist");
	Check(std::filesystem::exists("CoverageHistory.csv"), "CoverageHistory.csv must exist");

	std::printf("  [OK] Coverage Test 3: Heatmap & Dashboard passed\n");
}

void TestDifferentialExecutionAndMismatches() {
	std::printf("  [Coverage Test 4] Testing Differential Execution Reporter & Variant Validation...\n");

	X86RuntimeBridge bridge(1024 * 1024);
	GuestCpuContext ctx;
	ctx.rsp = 0x7FFFFFFF0000ULL;
	ctx.rip = 0x140001000ULL;

	std::vector<uint8_t> nop_ret = { 0x90, 0xC3 };
	InstructionMismatchReport report = DifferentialExecutionReporter::ValidateInstructionVariant(bridge, nop_ret, ctx);
	Check(!report.has_mismatch, "Valid NOP instruction must have 0 mismatches");

	std::printf("  [OK] Coverage Test 4: Differential Execution Reporter passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 ARM64 JIT Opcode Coverage Framework Suite  \n");
	std::printf("====================================================\n");

	TestOpcodeInventoryAndJsonExport();
	TestAutoTestVariantGenerator();
	TestCoverageHeatmapAndDashboardGenerator();
	TestDifferentialExecutionAndMismatches();

	std::printf("\nALL OPCODE COVERAGE FRAMEWORK TESTS PASSED!\n");
	return 0;
}
