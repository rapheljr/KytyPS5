// realAppCompatibilityEngine.h
//
// Real-World Application Compatibility & 5-Domain Differential Verification Engine.
//
// Validates translated ARM64 JIT execution against reference x86 execution for
// real application workloads across 5 state domains:
//   1. stdout / output buffers
//   2. memory / heap & stack states
//   3. registers (GPR, RFLAGS, SIMD)
//   4. exit code
//   5. exceptions & fault handling
//
// Supported Applications:
//   - Hello World (std I/O, string formatting, syscall bridge)
//   - SQLite (B-Tree indexing, binary search, memory transactions)
//   - zlib (DEFLATE LZ77 compression, Adler32 checksum, bitstream)
//   - libpng (PNG header decoding, Paeth filtering, RGBA conversion)
//   - SDL (Pixel blitting, alpha blending, event queue handling)
//   - Lua (Bytecode interpreter loop, stack tables, GC vectors)
//   - OpenSSL (AES-CTR/CBC round keys, SHA-256 compression, bignum)

#ifndef LOADER_RECOMPILER_REAL_APP_COMPATIBILITY_ENGINE_H
#define LOADER_RECOMPILER_REAL_APP_COMPATIBILITY_ENGINE_H

#include "common/common.h"
#include "loader/recompiler/arm64JitValidationFramework.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Loader::Recompiler {

// ─── Application Identifiers ──────────────────────────────────────────────────

enum class RealAppId : uint8_t {
	HelloWorld = 0,
	SQLite,
	Zlib,
	LibPNG,
	SDL,
	Lua,
	OpenSSL,
	Count
};

// ─── Application Workload Definition ──────────────────────────────────────────

struct RealAppWorkload {
	RealAppId             id                  = RealAppId::HelloWorld;
	std::string           name;
	std::string           category;
	std::string           description;
	std::vector<uint8_t>  x86_code_bytes;
	GuestCpuContext       initial_ctx{};
	std::vector<uint8_t>  input_buffer;
	int64_t               expected_exit_code  = 0;
	std::string           expected_stdout;
};

// ─── Execution Result (Captured State) ────────────────────────────────────────

struct AppExecutionResult {
	std::string           stdout_output;
	std::vector<uint8_t>  memory_state;
	GuestCpuContext       cpu_context{};
	int64_t               exit_code           = 0;
	bool                  exception_occurred  = false;
	uint32_t              exception_code      = 0;
	double                execution_time_ns   = 0.0;
	uint64_t              instruction_count   = 0;
};

// ─── 5-Domain Differential Match Result ───────────────────────────────────────

struct RealAppDiffResult {
	std::string           app_name;
	std::string           category;
	bool                  stdout_match        = true;
	bool                  memory_match        = true;
	bool                  registers_match     = true;
	bool                  exit_code_match     = true;
	bool                  exceptions_match    = true;
	bool                  overall_passed      = true;
	std::string           mismatch_details;
	double                native_time_ns      = 0.0;
	double                arm64_jit_time_ns   = 0.0;
	double                speedup_ratio       = 1.0;
	uint64_t              instruction_count   = 0;
};

// ─── Compatibility Engine ─────────────────────────────────────────────────────

class RealAppCompatibilityEngine {
public:
	RealAppCompatibilityEngine() = default;
	~RealAppCompatibilityEngine() = default;

	KYTY_CLASS_NO_COPY(RealAppCompatibilityEngine);

	// Load built-in workload definition for a target application
	[[nodiscard]] static RealAppWorkload LoadWorkload(RealAppId app_id);

	// Execute workload using reference x86 interpreter/bridge
	[[nodiscard]] AppExecutionResult ExecuteNativeX86(const RealAppWorkload& workload);

	// Execute workload using ARM64 JIT compiler + optimization pipeline
	[[nodiscard]] AppExecutionResult ExecuteArm64Jit(const RealAppWorkload& workload);

	// Compare results across all 5 domains
	[[nodiscard]] RealAppDiffResult CompareResults(
		const RealAppWorkload& workload,
		const AppExecutionResult& native_res,
		const AppExecutionResult& jit_res) noexcept;

	// Run full verification loop for an application
	[[nodiscard]] RealAppDiffResult VerifyApplication(RealAppId app_id);

	// Run full verification loop for all supported applications
	[[nodiscard]] std::vector<RealAppDiffResult> VerifyAllApplications();

	// Report Generators
	static bool GenerateHtmlReport(
		const std::vector<RealAppDiffResult>& results,
		const std::string& filepath);

	static bool GenerateMarkdownReport(
		const std::vector<RealAppDiffResult>& results,
		const std::string& filepath);

	static void PrintTerminalSummary(
		const std::vector<RealAppDiffResult>& results);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_REAL_APP_COMPATIBILITY_ENGINE_H
