// arm64JitValidationFramework.h
//
// Complete ARM64 JIT Validation Framework & High-Scale Differential Execution Comparator.

#ifndef LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H
#define LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H

#include "common/common.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace Loader::Recompiler {

struct StateDifferentialResult {
	bool gpr_match        = true;
	bool flags_match      = true;
	bool simd_match       = true;
	bool memory_match     = true;
	bool exceptions_match = true;
	bool branches_match   = true;
	bool overall_passed   = true;

	std::string mismatch_reason;
	std::string gpr_diff_hex;
	std::string simd_diff_hex;
	std::string memory_diff_hex;
	uint32_t    first_mismatch_byte_offset = 0;
};

struct ValidationStats {
	uint64_t total_programs_tested     = 0;
	uint64_t total_instructions_tested = 0;
	uint64_t total_passed              = 0;
	uint64_t total_failed              = 0;
	double   avg_translation_latency_ns = 0.0;
	double   avg_execution_latency_ns   = 0.0;

	std::array<bool, 256>     opcode_coverage{};
	std::array<uint64_t, 256> opcode_test_counts{};
	std::array<uint64_t, 256> opcode_fail_counts{};
};

class RandomProgramGenerator {
public:
	explicit RandomProgramGenerator(uint32_t seed = 1337);

	[[nodiscard]] std::vector<uint8_t> GenerateProgram(size_t instruction_count);

private:
	uint32_t m_rng_state;
	uint32_t NextRandom() noexcept;
};

class ByteLevelStateComparator {
public:
	[[nodiscard]] static StateDifferentialResult CompareStates(
		const GuestCpuContext& actual,
		const GuestCpuContext& expected,
		const uint8_t* stack_actual = nullptr,
		const uint8_t* stack_expected = nullptr,
		size_t stack_size = 0) noexcept;
};

class StateDifferentialComparator {
public:
	[[nodiscard]] static StateDifferentialResult CompareStates(const GuestCpuContext& actual, const GuestCpuContext& expected) noexcept;
};

class DifferentialVerifierEngine {
public:
	explicit DifferentialVerifierEngine(size_t cache_size = 16 * 1024 * 1024);
	~DifferentialVerifierEngine() = default;

	KYTY_CLASS_NO_COPY(DifferentialVerifierEngine);

	StateDifferentialResult VerifyStream(const uint8_t* code_ptr, size_t size_bytes, GuestCpuContext& ctx);

	void RecordOpcodeTested(uint8_t opcode, bool passed) noexcept;
	[[nodiscard]] const ValidationStats& GetStats() const noexcept { return m_stats; }
	void MergeStats(const ValidationStats& other) noexcept;

private:
	X86RuntimeBridge m_bridge;
	ValidationStats  m_stats{};
};

class TestCaseMinimizer {
public:
	static std::vector<uint8_t> MinimizeFailingSequence(
		const std::vector<uint8_t>& code_bytes,
		DifferentialVerifierEngine& engine,
		GuestCpuContext& initial_ctx);

	static bool GenerateReproducerCpp(
		const std::vector<uint8_t>& minimized_bytes,
		const StateDifferentialResult& result,
		const std::string& filepath);
};

class ParallelDifferentialRunner {
public:
	static ValidationStats RunParallelVerification(
		uint64_t target_instructions,
		size_t thread_count = 0);
};

class DifferentialHtmlReportGenerator {
public:
	static bool GenerateReport(const ValidationStats& stats, const std::string& filepath);
};

class ValidationDashboardGenerator {
public:
	static bool GenerateReport(const ValidationStats& stats, const std::string& filepath);
	static void PrintTerminalDashboard(const ValidationStats& stats);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H
