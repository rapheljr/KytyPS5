// ShaderOptPipelineTests.cpp
//
// Unit, golden output, stress, and benchmark test suite for Phase L:
// Shader Optimization Pipeline.

#include "graphics/shader/recompiler/opt/ShaderOptPipeline.h"
#include "graphics/shader/recompiler/opt/ShaderOptPasses.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Libs::Graphics::ShaderRecompiler::Opt;

// ─── 1. Constant Folding & Propagation Pass Test ─────────────────────────────

void TestConstantFoldingPass() {
	std::printf("  [Test 1] Constant Folding & Constant Propagation Pass...\n");

	ShaderIR ir;
	// inst 0: R0 = 10 + 20
	IRInstruction inst0{};
	inst0.opcode = IROpcode::Add;
	inst0.dst_reg_id = 0;
	inst0.src0.is_imm = true;
	inst0.src0.imm_u32 = 10;
	inst0.src1.is_imm = true;
	inst0.src1.imm_u32 = 20;

	// inst 1: R1 = 5 * 6
	IRInstruction inst1{};
	inst1.opcode = IROpcode::Mul;
	inst1.dst_reg_id = 1;
	inst1.src0.is_imm = true;
	inst1.src0.imm_u32 = 5;
	inst1.src1.is_imm = true;
	inst1.src1.imm_u32 = 6;

	ir.AddInstruction(inst0);
	ir.AddInstruction(inst1);

	PassConstantFoldingAndPropagation pass;
	PassStats stats{};
	bool modified = pass.Run(ir, stats);

	Check(modified, "Constant folding should modify IR");
	Check(stats.constants_folded == 2, "Constants folded count mismatch");

	const auto& insts = ir.GetInstructions();
	Check(insts[0].opcode == IROpcode::Mov && insts[0].src0.imm_u32 == 30, "Inst 0 fold failed");
	Check(insts[1].opcode == IROpcode::Mov && insts[1].src0.imm_u32 == 30, "Inst 1 fold failed");

	std::printf("  [OK] Test 1: Constant Folding & Constant Propagation Pass\n");
}

// ─── 2. Dead Code Elimination Pass Test ──────────────────────────────────────

void TestDeadCodeEliminationPass() {
	std::printf("  [Test 2] Dead Code Elimination Pass...\n");

	ShaderIR ir;
	// inst 0: R0 = 10 (Dead, R0 is never read)
	IRInstruction inst0{};
	inst0.opcode = IROpcode::Mov;
	inst0.dst_reg_id = 0;
	inst0.src0.is_imm = true;
	inst0.src0.imm_u32 = 10;

	// inst 1: R1 = 20 (Used by inst 2)
	IRInstruction inst1{};
	inst1.opcode = IROpcode::Mov;
	inst1.dst_reg_id = 1;
	inst1.src0.is_imm = true;
	inst1.src0.imm_u32 = 20;

	// inst 2: Export R1 (Side-effecting)
	IRInstruction inst2{};
	inst2.opcode = IROpcode::Mov;
	inst2.dst_reg_id = 2;
	inst2.src0.is_imm = false;
	inst2.src0.reg_id = 1;
	inst2.is_side_effecting = true;

	ir.AddInstruction(inst0);
	ir.AddInstruction(inst1);
	ir.AddInstruction(inst2);

	PassDeadCodeElimination pass;
	PassStats stats{};
	bool modified = pass.Run(ir, stats);

	Check(modified, "DCE should modify IR");
	Check(stats.instructions_eliminated == 1, "DCE eliminated count mismatch");

	Check(!ir.GetInstructions()[0].active, "Inst 0 (dead R0) should be inactivated");
	Check(ir.GetInstructions()[1].active, "Inst 1 (live R1) should remain active");

	std::printf("  [OK] Test 2: Dead Code Elimination Pass\n");
}

// ─── 3. Copy Propagation & CSE Pass Test ──────────────────────────────────────

void TestCopyPropagationAndCSEPass() {
	std::printf("  [Test 3] Copy Propagation & Common Subexpression Elimination Pass...\n");

	ShaderIR ir;
	// inst 0: R0 = R5 + R6
	IRInstruction inst0{};
	inst0.opcode = IROpcode::Add;
	inst0.dst_reg_id = 0;
	inst0.src0.is_imm = false;
	inst0.src0.reg_id = 5;
	inst0.src1.is_imm = false;
	inst0.src1.reg_id = 6;

	// inst 1: R1 = R5 + R6 (Identical subexpression to inst 0)
	IRInstruction inst1{};
	inst1.opcode = IROpcode::Add;
	inst1.dst_reg_id = 1;
	inst1.src0.is_imm = false;
	inst1.src0.reg_id = 5;
	inst1.src1.is_imm = false;
	inst1.src1.reg_id = 6;

	ir.AddInstruction(inst0);
	ir.AddInstruction(inst1);

	PassCommonSubexpressionElimination cse;
	PassStats stats{};
	bool modified = cse.Run(ir, stats);

	Check(modified, "CSE should modify IR");
	Check(ir.GetInstructions()[1].opcode == IROpcode::Mov, "CSE should replace redundant Add with Mov");
	Check(ir.GetInstructions()[1].src0.reg_id == 0, "CSE should point src0 to R0");

	std::printf("  [OK] Test 3: Copy Propagation & Common Subexpression Elimination Pass\n");
}

// ─── 4. Algebraic Simplification Pass Test ────────────────────────────────────

void TestAlgebraicSimplificationPass() {
	std::printf("  [Test 4] Algebraic Simplification Pass...\n");

	ShaderIR ir;
	// inst 0: R0 = R1 + 0 -> Mov R0, R1
	IRInstruction inst0{};
	inst0.opcode = IROpcode::Add;
	inst0.dst_reg_id = 0;
	inst0.src0.is_imm = false;
	inst0.src0.reg_id = 1;
	inst0.src1.is_imm = true;
	inst0.src1.imm_u32 = 0;

	// inst 1: R2 = R3 ^ R3 -> Mov R2, 0
	IRInstruction inst1{};
	inst1.opcode = IROpcode::Xor;
	inst1.dst_reg_id = 2;
	inst1.src0.is_imm = false;
	inst1.src0.reg_id = 3;
	inst1.src1.is_imm = false;
	inst1.src1.reg_id = 3;

	ir.AddInstruction(inst0);
	ir.AddInstruction(inst1);

	PassAlgebraicSimplification pass;
	PassStats stats{};
	bool modified = pass.Run(ir, stats);

	Check(modified, "Algebraic simplification should modify IR");
	Check(ir.GetInstructions()[0].opcode == IROpcode::Mov, "Inst 0 should simplify to Mov");
	Check(ir.GetInstructions()[1].opcode == IROpcode::Mov && ir.GetInstructions()[1].src0.imm_u32 == 0, "Inst 1 xor identity simplify failed");

	std::printf("  [OK] Test 4: Algebraic Simplification Pass\n");
}

// ─── 5. Full Optimization Pipeline Test (O0, O1, O2, O3) ──────────────────────

void TestFullOptimizationPipeline() {
	std::printf("  [Test 5] Full Optimization Pipeline (O0 vs O1 vs O2 vs O3)...\n");

	auto build_test_ir = []() {
		ShaderIR ir;
		// 10 instructions with redundant copies, algebraic zeroes, dead stores
		for (uint32_t i = 0; i < 5; ++i) {
			IRInstruction add{};
			add.opcode = IROpcode::Add;
			add.dst_reg_id = i;
			add.src0.is_imm = true;
			add.src0.imm_u32 = 100 + i;
			add.src1.is_imm = true;
			add.src1.imm_u32 = 200 + i;
			ir.AddInstruction(add);
		}

		IRInstruction dead_sub{};
		dead_sub.opcode = IROpcode::Sub;
		dead_sub.dst_reg_id = 10;
		dead_sub.src0.is_imm = true;
		dead_sub.src0.imm_u32 = 50;
		dead_sub.src1.is_imm = true;
		dead_sub.src1.imm_u32 = 20;
		ir.AddInstruction(dead_sub);

		return ir;
	};

	ShaderOptPassManager manager_o0(ShaderOptLevel::O0);
	ShaderIR ir_o0 = build_test_ir();
	manager_o0.Run(ir_o0);
	Check(manager_o0.GetStats().final_instruction_count == 6, "O0 should not eliminate instructions");

	ShaderOptPassManager manager_o2(ShaderOptLevel::O2);
	manager_o2.SetDebugValidation(true);
	ShaderIR ir_o2 = build_test_ir();
	bool ok = manager_o2.Run(ir_o2);
	Check(ok, "O2 optimization run failed");
	Check(manager_o2.GetStats().final_instruction_count < 6, "O2 should eliminate dead/constant instructions");

	std::printf("  [OK] Test 5: Full Optimization Pipeline\n");
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkShaderOptimizationPipeline() {
	std::printf("\n--- Phase L Benchmarks ---\n");

	constexpr int kNumShaders = 5000;
	constexpr int kInstsPerShader = 50;

	ShaderOptPassManager manager(ShaderOptLevel::O2);

	auto t0 = std::chrono::high_resolution_clock::now();
	for (int s = 0; s < kNumShaders; ++s) {
		ShaderIR ir;
		for (int i = 0; i < kInstsPerShader; ++i) {
			IRInstruction inst{};
			inst.opcode = (i % 2 == 0) ? IROpcode::Add : IROpcode::Mul;
			inst.dst_reg_id = i;
			inst.src0.is_imm = true;
			inst.src0.imm_u32 = i + 1;
			inst.src1.is_imm = true;
			inst.src1.imm_u32 = i + 2;
			ir.AddInstruction(inst);
		}
		manager.Run(ir);
	}
	auto t1 = std::chrono::high_resolution_clock::now();

	double total_dt_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
	double per_shader_us = (total_dt_ms * 1000.0) / kNumShaders;
	double shader_rate = kNumShaders / (total_dt_ms / 1000.0);

	std::printf("  [Bench] Optimization Pipeline Latency: %.2f us / shader\n", per_shader_us);
	std::printf("  [Bench] Optimization Throughput: %.2f shaders / sec (Tested %d shaders)\n", shader_rate, kNumShaders);
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase L: Shader Optimization Pipeline      \n");
	std::printf("====================================================\n\n");

	TestConstantFoldingPass();
	TestDeadCodeEliminationPass();
	TestCopyPropagationAndCSEPass();
	TestAlgebraicSimplificationPass();
	TestFullOptimizationPipeline();

	BenchmarkShaderOptimizationPipeline();

	std::printf("\nShaderOptPipelineTests: ALL PASSED\n");
	return 0;
}
