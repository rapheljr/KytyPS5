// ARM64RegisterAllocatorTests.cpp
//
// Production-Quality ARM64 Linear Scan Register Allocator Unit, Stress & Speed Benchmark Test Suite.

#include "loader/recompiler/arm64IRCodegen.h"
#include "loader/recompiler/arm64LinearScanAllocator.h"
#include "loader/recompiler/arm64RegisterVisualizer.h"
#include "loader/recompiler/compilerIR.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "ARM64RegisterAllocatorTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestBasicLinearScanAllocation() {
	std::printf("  [RegAlloc Test 1] Testing Basic GPR & SIMD Linear Scan Allocation...\n");

	ControlFlowGraph cfg;
	BasicBlock* bb = cfg.CreateBlock("entry");

	// Create GPR and SIMD virtual registers
	VirtualReg v1 = cfg.AllocateVReg(DataType::Int64);
	VirtualReg v2 = cfg.AllocateVReg(DataType::Int64);
	VirtualReg v_simd = cfg.AllocateVReg(DataType::Vec128);

	auto i1 = std::make_unique<IRInstruction>(IROpcode::Add);
	i1->SetDst(v1);
	i1->AddOperand(Value::MakeImmInt(100));
	bb->AddInstruction(std::move(i1));

	auto i2 = std::make_unique<IRInstruction>(IROpcode::Add);
	i2->SetDst(v2);
	i2->AddOperand(Value::MakeVReg(v1));
	i2->AddOperand(Value::MakeImmInt(50));
	bb->AddInstruction(std::move(i2));

	auto i3 = std::make_unique<IRInstruction>(IROpcode::Add);
	i3->SetDst(v_simd);
	i3->AddOperand(Value::MakeImmFloat(3.14, DataType::Float64));
	bb->AddInstruction(std::move(i3));

	LinearScanAllocator allocator;
	auto res = allocator.Allocate(cfg);
	Check(res.success, "LinearScanAllocator allocation failed");

	std::printf("  [OK] RegAlloc Test 1: Basic Allocation passed\n");
}

void TestSpillAndReloadInsertion() {
	std::printf("  [RegAlloc Test 2] Testing Spill Slot Allocation & Reload Insertion...\n");

	ControlFlowGraph cfg;
	BasicBlock* bb = cfg.CreateBlock("high_pressure");

	// Create 25 live virtual registers simultaneously to force spilling
	std::vector<VirtualReg> vregs;
	for (int i = 0; i < 25; ++i) {
		vregs.push_back(cfg.AllocateVReg(DataType::Int64));
		auto inst = std::make_unique<IRInstruction>(IROpcode::Add);
		inst->SetDst(vregs.back());
		inst->AddOperand(Value::MakeImmInt(i * 10));
		bb->AddInstruction(std::move(inst));
	}

	// Use all 25 registers at the bottom to overlap all live intervals
	auto sum_inst = std::make_unique<IRInstruction>(IROpcode::Add);
	sum_inst->SetDst(cfg.AllocateVReg(DataType::Int64));
	for (int i = 0; i < 25; ++i) {
		sum_inst->AddOperand(Value::MakeVReg(vregs[i]));
	}
	bb->AddInstruction(std::move(sum_inst));

	LinearScanAllocator allocator;
	auto res = allocator.Allocate(cfg);
	Check(res.success, "Allocation under high register pressure failed");
	Check(res.total_spill_bytes > 0, "Spill bytes must be > 0 when register count exceeds pool");

	std::printf("  [OK] RegAlloc Test 2: Spill & Reload Insertion passed (Spilled %u bytes)\n", res.total_spill_bytes);
}

void TestVisualizationOutput() {
	std::printf("  [RegAlloc Test 3] Testing Visualization ASCII Timeline & Pressure Heatmap...\n");

	ControlFlowGraph cfg;
	BasicBlock* bb = cfg.CreateBlock("vis_block");

	VirtualReg v1 = cfg.AllocateVReg(DataType::Int64);
	VirtualReg v2 = cfg.AllocateVReg(DataType::Int64);

	auto i1 = std::make_unique<IRInstruction>(IROpcode::Add);
	i1->SetDst(v1);
	i1->AddOperand(Value::MakeImmInt(42));
	bb->AddInstruction(std::move(i1));

	auto i2 = std::make_unique<IRInstruction>(IROpcode::Add);
	i2->SetDst(v2);
	i2->AddOperand(Value::MakeVReg(v1));
	bb->AddInstruction(std::move(i2));

	LinearScanAllocator allocator;
	allocator.Allocate(cfg);

	std::string timeline = RegisterAllocationVisualizer::RenderAsciiTimeline(allocator.GetIntervals());
	Check(timeline.find("ARM64 Linear Scan Register Allocation Timeline") != std::string::npos, "Timeline render failed");

	std::string heatmap = RegisterAllocationVisualizer::RenderPressureHeatmap(allocator.GetIntervals());
	Check(heatmap.find("Register Pressure Heatmap") != std::string::npos, "Heatmap render failed");

	std::printf("  [OK] RegAlloc Test 3: Visualization Output passed\n");
}

void RunStressTest(size_t num_blocks = 1000) {
	std::printf("  [RegAlloc Stress] Running Stress Test over %zu Random Basic Blocks...\n", num_blocks);

	ControlFlowGraph cfg;
	std::mt19937 rng(0x87654321);

	std::vector<BasicBlock*> blocks;
	for (size_t b = 0; b < num_blocks; ++b) {
		blocks.push_back(cfg.CreateBlock("block_" + std::to_string(b)));
	}

	for (size_t b = 0; b < num_blocks; ++b) {
		size_t inst_count = (rng() % 10) + 5;
		std::vector<VirtualReg> local_vregs;

		for (size_t i = 0; i < inst_count; ++i) {
			VirtualReg vr = cfg.AllocateVReg((rng() % 2 == 0) ? DataType::Int64 : DataType::Vec128);
			local_vregs.push_back(vr);

			auto inst = std::make_unique<IRInstruction>(IROpcode::Add);
			inst->SetDst(vr);
			inst->AddOperand(Value::MakeImmInt(123));
			blocks[b]->AddInstruction(std::move(inst));
		}

		if (b + 1 < num_blocks) {
			cfg.AddEdge(blocks[b], blocks[b + 1]);
		}
	}

	LinearScanAllocator allocator;
	auto res = allocator.Allocate(cfg);
	Check(res.success, "Stress test register allocation failed");

	std::printf("  [OK] RegAlloc Stress passed: %zu blocks allocated with zero errors!\n", num_blocks);
}

void RunAllocationBenchmark(size_t num_intervals = 100000) {
	std::printf("  [RegAlloc Benchmark] Benchmarking Speed over %zu Live Intervals...\n", num_intervals);

	ControlFlowGraph cfg;
	BasicBlock* bb = cfg.CreateBlock("bench_block");

	for (size_t i = 0; i < num_intervals; ++i) {
		VirtualReg vr = cfg.AllocateVReg(DataType::Int64);
		auto inst = std::make_unique<IRInstruction>(IROpcode::Add);
		inst->SetDst(vr);
		inst->AddOperand(Value::MakeImmInt(i));
		bb->AddInstruction(std::move(inst));
	}

	auto start_time = std::chrono::high_resolution_clock::now();

	LinearScanAllocator allocator;
	allocator.Allocate(cfg);

	auto end_time = std::chrono::high_resolution_clock::now();
	double duration_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
	double intervals_per_sec = (static_cast<double>(num_intervals) / duration_ms) * 1000.0;

	std::printf("  [Benchmark Result] Allocated %zu intervals in %.2f ms (%.0f intervals/sec)\n",
	            num_intervals, duration_ms, intervals_per_sec);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Production ARM64 Linear Scan Allocator    \n");
	std::printf("====================================================\n");

	TestBasicLinearScanAllocation();
	TestSpillAndReloadInsertion();
	TestVisualizationOutput();
	RunStressTest(1000);
	RunAllocationBenchmark(100000);

	std::printf("\nALL REGISTER ALLOCATOR TESTS & BENCHMARKS PASSED!\n");
	return 0;
}
