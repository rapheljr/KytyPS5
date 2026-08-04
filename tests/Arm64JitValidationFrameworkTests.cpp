// Arm64JitValidationFrameworkTests.cpp
//
// Complete Test Suite for ARM64 JIT Validation Framework & Differential Execution Comparator.

#include "loader/recompiler/arm64JitValidationFramework.h"

#include <cstdio>
#include <cstdlib>

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
	std::vector<uint8_t> prog = gen.GenerateProgram(50);

	Check(!prog.empty(), "Generated program must not be empty");
	Check(prog.back() == 0xC3, "Generated program must end with RET (0xC3)");

	std::printf("  [OK] Validation Test 1: Random Program Generator passed\n");
}

void TestStateDifferentialComparator() {
	std::printf("  [Validation Test 2] Testing 6-Domain State Differential Comparator...\n");

	GuestCpuContext ctx1;
	GuestCpuContext ctx2;

	ctx1.rax = 0x12345678ULL;
	ctx2.rax = 0x12345678ULL;

	StateDifferentialResult res1 = StateDifferentialComparator::CompareStates(ctx1, ctx2);
	Check(res1.overall_passed, "Matching contexts must pass differential check");

	ctx2.rax = 0xDEADBEEFULL; // Inject GPR mismatch
	StateDifferentialResult res2 = StateDifferentialComparator::CompareStates(ctx1, ctx2);
	Check(!res2.gpr_match, "GPR mismatch must be detected");
	Check(!res2.overall_passed, "Context with GPR mismatch must fail");

	std::printf("  [OK] Validation Test 2: 6-Domain Differential Comparator passed\n");
}

void TestDifferentialVerifierEngineAndDashboard() {
	std::printf("  [Validation Test 3] Testing Differential Verifier Engine & Fuzzing 1,000 Programs...\n");

	DifferentialVerifierEngine engine(1024 * 1024);
	RandomProgramGenerator gen(1337);

	const size_t PROGRAM_COUNT = 1000;
	for (size_t i = 0; i < PROGRAM_COUNT; ++i) {
		std::vector<uint8_t> prog = gen.GenerateProgram(10);
		GuestCpuContext ctx;
		ctx.rip = 0x140001000ULL + i * 16;
		ctx.rsp = 0x7FFFFFFF0000ULL;

		StateDifferentialResult diff = engine.VerifyStream(prog.data(), prog.size(), ctx);
		Check(diff.overall_passed, "All random test programs must pass verification");
	}

	const auto& stats = engine.GetStats();
	Check(stats.total_programs_tested == PROGRAM_COUNT, "Total programs count mismatch");
	Check(stats.total_passed == PROGRAM_COUNT, "All programs must pass validation");

	ValidationDashboardGenerator::PrintTerminalDashboard(stats);
	bool saved = ValidationDashboardGenerator::GenerateReport(stats, "ARM64_JIT_VALIDATION_REPORT.md");
	Check(saved, "Validation report export failed");

	std::printf("  [OK] Validation Test 3: Differential Verifier & Dashboard passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 ARM64 JIT Validation Framework Suite       \n");
	std::printf("====================================================\n");

	TestRandomProgramGenerator();
	TestStateDifferentialComparator();
	TestDifferentialVerifierEngineAndDashboard();

	std::printf("\nALL ARM64 JIT VALIDATION TESTS PASSED!\n");
	return 0;
}
