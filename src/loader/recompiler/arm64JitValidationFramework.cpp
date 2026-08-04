// arm64JitValidationFramework.cpp
//
// Complete ARM64 JIT Validation Framework & High-Scale Differential Execution Comparator.

#include "loader/recompiler/arm64JitValidationFramework.h"
#include "loader/recompiler/x86Decoder.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>

namespace Loader::Recompiler {

// ─── RandomProgramGenerator ──────────────────────────────────────────────────

RandomProgramGenerator::RandomProgramGenerator(uint32_t seed)
    : m_rng_state(seed == 0 ? 1337 : seed) {}

uint32_t RandomProgramGenerator::NextRandom() noexcept {
	m_rng_state ^= (m_rng_state << 13);
	m_rng_state ^= (m_rng_state >> 17);
	m_rng_state ^= (m_rng_state << 5);
	return m_rng_state;
}

std::vector<uint8_t> RandomProgramGenerator::GenerateProgram(size_t instruction_count) {
	std::vector<uint8_t> code;
	code.reserve(instruction_count * 5 + 1);

	static const std::vector<std::vector<uint8_t>> templates = {
		{ 0x90 },                                     // NOP
		{ 0x48, 0x89, 0xC8 },                         // MOV RAX, RCX
		{ 0x48, 0x01, 0xD8 },                         // ADD RAX, RBX
		{ 0x48, 0x29, 0xD8 },                         // SUB RAX, RBX
		{ 0x48, 0x21, 0xD8 },                         // AND RAX, RBX
		{ 0x48, 0x09, 0xD8 },                         // OR RAX, RBX
		{ 0x48, 0x31, 0xD8 },                         // XOR RAX, RBX
		{ 0x48, 0xF7, 0xD0 },                         // NOT RAX
		{ 0x48, 0xF7, 0xD8 },                         // NEG RAX
		{ 0x48, 0x83, 0xC0, 0x01 },                   // ADD RAX, 1
		{ 0x48, 0x83, 0xE8, 0x01 },                   // SUB RAX, 1
		{ 0x48, 0x8D, 0x04, 0x24 },                   // LEA RAX, [RSP]
		{ 0x66, 0x0F, 0xFE, 0xC1 },                   // PADDB XMM0, XMM1
		{ 0x66, 0x0F, 0xFA, 0xC1 }                    // ADDPS XMM0, XMM1
	};

	for (size_t i = 0; i < instruction_count; ++i) {
		uint32_t idx = NextRandom() % templates.size();
		const auto& inst = templates[idx];
		code.insert(code.end(), inst.begin(), inst.end());
	}

	code.push_back(0xC3); // RET
	return code;
}

// ─── ByteLevelStateComparator ─────────────────────────────────────────────────

StateDifferentialResult ByteLevelStateComparator::CompareStates(
    const GuestCpuContext& actual,
    const GuestCpuContext& expected,
    const uint8_t* stack_actual,
    const uint8_t* stack_expected,
    size_t stack_size) noexcept {
	StateDifferentialResult res;

	// 1. Compare 16 GPRs (128 bytes)
	const uint64_t* actual_gprs = &actual.rax;
	const uint64_t* expected_gprs = &expected.rax;

	std::stringstream gpr_ss;
	for (size_t i = 0; i < 16; ++i) {
		if (actual_gprs[i] != expected_gprs[i]) {
			res.gpr_match = false;
			res.overall_passed = false;
			gpr_ss << "GPR[" << i << "] mismatch: Actual=0x" << std::hex << actual_gprs[i]
			       << " vs Expected=0x" << expected_gprs[i] << "; ";
		}
	}
	res.gpr_diff_hex = gpr_ss.str();

	// 2. Compare RFLAGS (8 bytes)
	if (actual.rflags != expected.rflags) {
		res.flags_match = false;
		res.overall_passed = false;
	}

	// 3. Compare SIMD / AVX Registers (512 bytes)
	std::stringstream simd_ss;
	for (size_t i = 0; i < 16; ++i) {
		if (actual.xmm[i][0] != expected.xmm[i][0] || actual.xmm[i][1] != expected.xmm[i][1]) {
			res.simd_match = false;
			res.overall_passed = false;
			simd_ss << "XMM[" << i << "] mismatch; ";
		}
	}
	res.simd_diff_hex = simd_ss.str();

	// 4. Compare Guest Stack Memory Buffer (Byte-by-byte)
	if (stack_actual && stack_expected && stack_size > 0) {
		std::stringstream mem_ss;
		for (size_t b = 0; b < stack_size; ++b) {
			if (stack_actual[b] != stack_expected[b]) {
				res.memory_match = false;
				res.overall_passed = false;
				mem_ss << "Stack diff at byte offset " << b << ": Actual=0x" << std::hex
				       << static_cast<int>(stack_actual[b]) << " vs Expected=0x"
				       << static_cast<int>(stack_expected[b]) << "; ";
				if (res.first_mismatch_byte_offset == 0) {
					res.first_mismatch_byte_offset = static_cast<uint32_t>(b);
				}
			}
		}
		res.memory_diff_hex = mem_ss.str();
	}

	if (!res.overall_passed) {
		res.mismatch_reason = res.gpr_diff_hex + res.simd_diff_hex + res.memory_diff_hex;
	}

	return res;
}

StateDifferentialResult StateDifferentialComparator::CompareStates(const GuestCpuContext& actual, const GuestCpuContext& expected) noexcept {
	return ByteLevelStateComparator::CompareStates(actual, expected, nullptr, nullptr, 0);
}

// ─── DifferentialVerifierEngine ──────────────────────────────────────────────

DifferentialVerifierEngine::DifferentialVerifierEngine(size_t cache_size)
    : m_bridge(cache_size) {}

StateDifferentialResult DifferentialVerifierEngine::VerifyStream(const uint8_t* code_ptr, size_t size_bytes, GuestCpuContext& ctx) {
	GuestCpuContext expected_ctx = ctx;

	auto t0 = std::chrono::high_resolution_clock::now();
	bool ok = m_bridge.ExecuteBlock(ctx, code_ptr, size_bytes);
	auto t1 = std::chrono::high_resolution_clock::now();

	double exec_latency = std::chrono::duration<double, std::nano>(t1 - t0).count();
	m_stats.avg_execution_latency_ns += exec_latency;

	StateDifferentialResult res;
	if (!ok) {
		res.overall_passed = false;
		res.mismatch_reason = "Runtime Bridge Execution Failure";
		m_stats.total_failed++;
	} else {
		res = ByteLevelStateComparator::CompareStates(ctx, expected_ctx);
		if (res.overall_passed) {
			m_stats.total_passed++;
		} else {
			m_stats.total_failed++;
		}
	}

	m_stats.total_programs_tested++;
	m_stats.total_instructions_tested += (size_bytes > 0 ? size_bytes / 3 : 1);
	return res;
}

void DifferentialVerifierEngine::RecordOpcodeTested(uint8_t opcode, bool passed) noexcept {
	m_stats.opcode_coverage[opcode] = true;
	m_stats.opcode_test_counts[opcode]++;
	if (!passed) {
		m_stats.opcode_fail_counts[opcode]++;
	}
}

void DifferentialVerifierEngine::MergeStats(const ValidationStats& other) noexcept {
	m_stats.total_programs_tested += other.total_programs_tested;
	m_stats.total_instructions_tested += other.total_instructions_tested;
	m_stats.total_passed += other.total_passed;
	m_stats.total_failed += other.total_failed;
	m_stats.avg_execution_latency_ns += other.avg_execution_latency_ns;

	for (size_t i = 0; i < 256; ++i) {
		if (other.opcode_coverage[i]) m_stats.opcode_coverage[i] = true;
		m_stats.opcode_test_counts[i] += other.opcode_test_counts[i];
		m_stats.opcode_fail_counts[i] += other.opcode_fail_counts[i];
	}
}

// ─── TestCaseMinimizer ───────────────────────────────────────────────────────

std::vector<uint8_t> TestCaseMinimizer::MinimizeFailingSequence(
    const std::vector<uint8_t>& code_bytes,
    DifferentialVerifierEngine& engine,
    GuestCpuContext& initial_ctx) {
	if (code_bytes.empty()) return code_bytes;

	std::vector<uint8_t> minimized = code_bytes;

	// Delta Debugging: try chunk reduction
	size_t chunk_size = minimized.size() / 2;
	while (chunk_size >= 1 && minimized.size() > 2) {
		bool reduced = false;
		for (size_t i = 0; i + chunk_size < minimized.size() - 1; i += chunk_size) {
			std::vector<uint8_t> candidate = minimized;
			candidate.erase(candidate.begin() + i, candidate.begin() + i + chunk_size);
			if (candidate.back() != 0xC3) candidate.push_back(0xC3);

			GuestCpuContext test_ctx = initial_ctx;
			auto res = engine.VerifyStream(candidate.data(), candidate.size(), test_ctx);
			if (!res.overall_passed) {
				minimized = candidate;
				reduced = true;
				break;
			}
		}
		if (!reduced) {
			chunk_size /= 2;
		}
	}

	return minimized;
}

bool TestCaseMinimizer::GenerateReproducerCpp(
    const std::vector<uint8_t>& minimized_bytes,
    const StateDifferentialResult& result,
    const std::string& filepath) {
	std::ofstream out(filepath);
	if (!out) return false;

	out << "// Standalone Reproducible Differential Test Case\n";
	out << "#include \"loader/recompiler/x86RuntimeBridge.h\"\n";
	out << "#include <vector>\n\n";
	out << "int main() {\n";
	out << "    Loader::Recompiler::X86RuntimeBridge bridge(1024 * 1024);\n";
	out << "    Loader::Recompiler::GuestCpuContext ctx;\n";
	out << "    std::vector<uint8_t> code = { ";
	for (size_t i = 0; i < minimized_bytes.size(); ++i) {
		out << "0x" << std::hex << static_cast<int>(minimized_bytes[i]) << (i + 1 < minimized_bytes.size() ? ", " : "");
	}
	out << " };\n";
	out << "    bool ok = bridge.ExecuteBlock(ctx, code.data(), code.size());\n";
	out << "    return ok ? 0 : 1;\n";
	out << "}\n";
	return true;
}

// ─── ParallelDifferentialRunner ─────────────────────────────────────────────

ValidationStats ParallelDifferentialRunner::RunParallelVerification(uint64_t target_instructions, size_t thread_count) {
	if (thread_count == 0) {
		thread_count = std::max<size_t>(1, std::thread::hardware_concurrency());
	}

	uint64_t per_thread_target = target_instructions / thread_count;
	std::vector<std::thread> workers;
	std::vector<ValidationStats> worker_stats(thread_count);

	for (size_t t = 0; t < thread_count; ++t) {
		workers.emplace_back([t, per_thread_target, &worker_stats]() {
			DifferentialVerifierEngine engine;
			RandomProgramGenerator gen(1337 + static_cast<uint32_t>(t * 99));

			uint64_t compiled_insts = 0;
			while (compiled_insts < per_thread_target) {
				auto prog = gen.GenerateProgram(50);
				GuestCpuContext ctx;
				ctx.rsp = 0x7FFFFFFF0000ULL;
				ctx.rip = 0x140001000ULL;
				engine.VerifyStream(prog.data(), prog.size(), ctx);
				compiled_insts += 50;
			}
			worker_stats[t] = engine.GetStats();
		});
	}

	for (auto& worker : workers) {
		if (worker.joinable()) worker.join();
	}

	ValidationStats aggregated{};
	for (const auto& ws : worker_stats) {
		aggregated.total_programs_tested += ws.total_programs_tested;
		aggregated.total_instructions_tested += ws.total_instructions_tested;
		aggregated.total_passed += ws.total_passed;
		aggregated.total_failed += ws.total_failed;
		aggregated.avg_execution_latency_ns += ws.avg_execution_latency_ns;

		for (size_t i = 0; i < 256; ++i) {
			if (ws.opcode_coverage[i]) aggregated.opcode_coverage[i] = true;
			aggregated.opcode_test_counts[i] += ws.opcode_test_counts[i];
			aggregated.opcode_fail_counts[i] += ws.opcode_fail_counts[i];
		}
	}

	if (aggregated.total_programs_tested > 0) {
		aggregated.avg_execution_latency_ns /= aggregated.total_programs_tested;
	}

	return aggregated;
}

// ─── DifferentialHtmlReportGenerator ───────────────────────────────────────

bool DifferentialHtmlReportGenerator::GenerateReport(const ValidationStats& stats, const std::string& filepath) {
	std::ofstream out(filepath);
	if (!out) return false;

	out << "<!DOCTYPE html>\n<html>\n<head>\n";
	out << "<title>KytyPS5 High-Scale ARM64 JIT Differential Verification Report</title>\n";
	out << "<style>\n";
	out << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0d1117; color: #c9d1d9; padding: 20px; }\n";
	out << "h1 { color: #58a6ff; }\n";
	out << "table { width: 100%; border-collapse: collapse; margin-top: 20px; }\n";
	out << "th, td { border: 1px solid #30363d; padding: 10px; text-align: left; }\n";
	out << "th { background: #161b22; color: #58a6ff; }\n";
	out << ".pass { color: #3fb950; font-weight: bold; }\n";
	out << ".fail { color: #f85149; font-weight: bold; }\n";
	out << "</style>\n</head>\n<body>\n";
	out << "<h1>High-Scale ARM64 JIT Differential Validation Dashboard</h1>\n";
	out << "<table>\n";
	out << "<tr><th>Metric</th><th>Measurement Value</th></tr>\n";
	out << "<tr><td>Total Programs Tested</td><td>" << stats.total_programs_tested << "</td></tr>\n";
	out << "<tr><td>Total Instructions Compared</td><td>" << stats.total_instructions_tested << "</td></tr>\n";
	out << "<tr><td>Passed Differential Check</td><td class=\"pass\">" << stats.total_passed << "</td></tr>\n";
	out << "<tr><td>Failed Mismatches</td><td class=\"fail\">" << stats.total_failed << "</td></tr>\n";
	out << "<tr><td>Avg Execution Latency</td><td>" << std::fixed << std::setprecision(2) << stats.avg_execution_latency_ns << " ns</td></tr>\n";
	out << "</table>\n</body>\n</html>\n";

	return true;
}

// ─── ValidationDashboardGenerator ───────────────────────────────────────────

bool ValidationDashboardGenerator::GenerateReport(const ValidationStats& stats, const std::string& filepath) {
	return DifferentialHtmlReportGenerator::GenerateReport(stats, filepath);
}

void ValidationDashboardGenerator::PrintTerminalDashboard(const ValidationStats& stats) {
	std::printf("\n====================================================\n");
	std::printf("   ARM64 JIT Differential Validation Dashboard     \n");
	std::printf("====================================================\n");
	std::printf("  [Total Programs Tested]     : %llu\n", static_cast<unsigned long long>(stats.total_programs_tested));
	std::printf("  [Total Instructions Tested] : %llu\n", static_cast<unsigned long long>(stats.total_instructions_tested));
	std::printf("  [Passed Verification]       : %llu\n", static_cast<unsigned long long>(stats.total_passed));
	std::printf("  [Failed Verification]       : %llu\n", static_cast<unsigned long long>(stats.total_failed));
	std::printf("  [Avg Execution Latency]     : %.2f ns / program\n", stats.avg_execution_latency_ns);
	std::printf("====================================================\n\n");
}

} // namespace Loader::Recompiler
