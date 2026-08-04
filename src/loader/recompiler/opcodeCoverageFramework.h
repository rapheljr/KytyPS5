// opcodeCoverageFramework.h
//
// Complete Self-Extending Opcode Coverage Framework & Test Variant Generator.

#ifndef LOADER_RECOMPILER_OPCODE_COVERAGE_FRAMEWORK_H
#define LOADER_RECOMPILER_OPCODE_COVERAGE_FRAMEWORK_H

#include "common/common.h"
#include "loader/recompiler/arm64JitValidationFramework.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Loader::Recompiler {

enum class OpcodeCategory : uint8_t {
	Arithmetic = 0,
	Logic,
	Shift,
	Rotate,
	Move,
	Compare,
	ControlFlow,
	Stack,
	String,
	SimdSSE,
	AVX,
	System,
	Count
};

enum class OpcodeStatus : uint8_t {
	Implemented = 0,
	PartiallyCovered,
	Unsupported
};

struct OpcodeMetadata {
	uint8_t        opcode_byte     = 0;
	bool           is_twobyte      = false;
	std::string    mnemonic;
	OpcodeCategory category        = OpcodeCategory::Arithmetic;
	uint8_t        operand_count   = 0;
	std::string    operand_types;
	uint8_t        imm_size_bytes  = 0;
	bool           modrm_usage     = false;
	std::string    flags_modified;
	bool           simd_usage      = false;
	std::string    privilege_level = "User";
	OpcodeStatus   status          = OpcodeStatus::Unsupported;
	uint32_t       variants_tested = 0;
	uint32_t       variants_passed = 0;
};

struct CategorySummary {
	OpcodeCategory category;
	std::string    name;
	size_t         total_opcodes       = 0;
	size_t         implemented_opcodes = 0;
	size_t         unsupported_opcodes = 0;
	double         coverage_pct        = 0.0;
};

struct InstructionMismatchReport {
	bool        has_mismatch = false;
	std::string instruction_hex;
	std::string disassembly;
	std::string expected_state;
	std::string actual_state;
	std::string ir_dump;
	std::string arm64_dump;
	std::string hex_diff;
};

class OpcodeInventory {
public:
	static std::vector<OpcodeMetadata> BuildInventory();
	static bool ExportJson(const std::vector<OpcodeMetadata>& inventory, const std::string& filepath);
	static bool ExportMarkdown(const std::vector<OpcodeMetadata>& inventory, const std::string& filepath);

	static std::map<OpcodeCategory, CategorySummary> ComputeSummaries(const std::vector<OpcodeMetadata>& inventory);
	static const char* CategoryToString(OpcodeCategory cat) noexcept;
};

class AutoTestGenerator {
public:
	static std::vector<std::vector<uint8_t>> GenerateVariantsForOpcode(const OpcodeMetadata& meta);
};

class DifferentialExecutionReporter {
public:
	static InstructionMismatchReport ValidateInstructionVariant(X86RuntimeBridge& bridge, const std::vector<uint8_t>& code_bytes, GuestCpuContext& ctx);
};

class CoverageHeatmapGenerator {
public:
	static bool GenerateHtmlHeatmap(const std::vector<OpcodeMetadata>& inventory, const std::string& filepath);
	static bool GenerateDashboardMarkdown(const std::vector<OpcodeMetadata>& inventory, const std::string& filepath);
	static bool AppendHistoryCsv(const std::vector<OpcodeMetadata>& inventory, const std::string& filepath);
};

class OpcodeCoverageCiRunner {
public:
	static bool RunCiVerification(double min_coverage_pct = 90.0);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_OPCODE_COVERAGE_FRAMEWORK_H
