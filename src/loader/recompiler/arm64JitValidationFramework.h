// arm64JitValidationFramework.h
//
// Complete ARM64 JIT Validation Framework & Differential Execution Comparator.

#ifndef LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H
#define LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H

#include "common/common.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <array>
#include <cstddef>
#include <cstdint>
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
};

class RandomProgramGenerator {
public:
	explicit RandomProgramGenerator(uint32_t seed = 1337);

	[[nodiscard]] std::vector<uint8_t> GenerateProgram(size_t instruction_count);

private:
	uint32_t m_rng_state;
	uint32_t NextRandom() noexcept;
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

	void RecordOpcodeTested(uint8_t opcode) noexcept;
	[[nodiscard]] const ValidationStats& GetStats() const noexcept { return m_stats; }

private:
	X86RuntimeBridge m_bridge;
	ValidationStats  m_stats{};
};

class ValidationDashboardGenerator {
public:
	static bool GenerateReport(const ValidationStats& stats, const std::string& filepath);
	static void PrintTerminalDashboard(const ValidationStats& stats);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_ARM64_JIT_VALIDATION_FRAMEWORK_H
