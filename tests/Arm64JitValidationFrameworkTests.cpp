// Arm64JitValidationFrameworkTests.cpp
//
// Test Suite for High-Scale ARM64 JIT Differential Testing & Minimization Framework.

#include "loader/recompiler/arm64JitValidationFramework.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "Arm64JitValidationFrameworkTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestRandomProgramGenerator() {
	std::printf("  [Validation Test 1] Testing Random Program Stream Generator...\n");
	RandomProgramGenerator gen(42);

	auto prog1 = gen.GenerateProgram(20);
	Check(!prog1.empty(), "Generated program 1 must not be empty");
	Check(prog1.back() == 0xC3, "Generated program must end with RET instruction (0xC3)");

	auto prog2 = gen.GenerateProgram(50);
	Check(prog2.size() > prog1.size(), "Program 2 must contain more instruction bytes than Program 1");

	std::printf("  [OK] Validation Test 1: Random Program Generator passed\n");
}

void TestByteLevelStateComparator() {
	std::printf("  [Validation Test 2] Testing Byte-Level State Comparator...\n");

	GuestCpuContext ctx1;
	GuestCpuContext ctx2;
	ctx1.rax = 0x123456789ABCDEF0ULL;
	ctx2.rax = 0x123456789ABCDEF0ULL;
	ctx1.rsp = 0x7FFFFFFF0000ULL;
	ctx2.rsp = 0x7FFFFFFF0000ULL;

	uint8_t stack_buf1[64] = { 0 };
	uint8_t stack_buf2[64] = { 0 };

	auto res = ByteLevelStateComparator::CompareStates(ctx1, ctx2, stack_buf1, stack_buf2, 64);
	Check(res.overall_passed, "Identical register and stack state must pass differential verification");

	ctx2.rax = 0x9999999999999999ULL;
	auto res_mismatch = ByteLevelStateComparator::CompareStates(ctx1, ctx2, stack_buf1, stack_buf2, 64);
	Check(!res_mismatch.overall_passed, "Modified RAX must trigger state mismatch");
	Check(!res_mismatch.gpr_match, "GPR match flag must be false on RAX diff");

	std::printf("  [OK] Validation Test 2: Byte-Level State Comparator passed\n");
}

void TestParallelDifferentialRunner() {
	std::printf("  [Validation Test 3] Testing Multi-Threaded Parallel Execution Across CPU Cores...\n");

	ValidationStats stats = ParallelDifferentialRunner::RunParallelVerification(10000, 0);
	Check(stats.total_instructions_tested >= 10000, "Parallel runner must reach or exceed target instruction count");
	Check(stats.total_failed == 0, "All random instruction streams must pass differential verification with zero failures");

	std::printf("  [OK] Validation Test 3: Parallel Runner passed (%llu instructions tested)\n",
	           static_cast<unsigned long long>(stats.total_instructions_tested));
}

void TestTestCaseMinimizerAndHtmlReport() {
	std::printf("  [Validation Test 4] Testing Test Case Minimizer & HTML Report Generator...\n");

	DifferentialVerifierEngine engine;
	GuestCpuContext ctx;
	ctx.rsp = 0x7FFFFFFF0000ULL;
	ctx.rip = 0x140001000ULL;

	std::vector<uint8_t> code = { 0x90, 0x48, 0x01, 0xC0, 0xC3 }; // NOP, ADD RAX, RAX, RET
	auto minimized = TestCaseMinimizer::MinimizeFailingSequence(code, engine, ctx);
	Check(!minimized.empty(), "Minimized sequence must not be empty");

	StateDifferentialResult diff;
	diff.overall_passed = false;
	diff.gpr_diff_hex = "RAX diff";

	bool repro_ok = TestCaseMinimizer::GenerateReproducerCpp(minimized, diff, "ReproducerTest.cpp");
	Check(repro_ok, "GenerateReproducerCpp must succeed");
	Check(std::filesystem::exists("ReproducerTest.cpp"), "ReproducerTest.cpp file must exist");

	bool html_ok = DifferentialHtmlReportGenerator::GenerateReport(engine.GetStats(), "DifferentialValidationReport.html");
	Check(html_ok, "DifferentialHtmlReportGenerator must succeed");
	Check(std::filesystem::exists("DifferentialValidationReport.html"), "DifferentialValidationReport.html file must exist");

	std::printf("  [OK] Validation Test 4: Minimizer & HTML Report Generator passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 ARM64 JIT Validation Framework Suite       \n");
	std::printf("====================================================\n");

	TestRandomProgramGenerator();
	TestByteLevelStateComparator();
	TestParallelDifferentialRunner();
	TestTestCaseMinimizerAndHtmlReport();

	std::printf("\nALL ARM64 JIT VALIDATION TESTS PASSED!\n");
	return 0;
}
