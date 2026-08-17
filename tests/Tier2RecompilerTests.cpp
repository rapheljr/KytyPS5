// Tier2RecompilerTests.cpp
//
// Unit & Integration tests for Tier-2 JIT Optimizations, GVN, Constant Folding,
// and Runtime Optimization Engine.

#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/jitTier2Optimizer.h"
#include "loader/recompiler/runtimeOptimizationEngine.h"

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>

namespace {

void Check(bool value, const char* msg) {
	if (!value) {
		std::printf("ASSERTION FAILED: %s\n", msg);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestConstantFolding() {
	std::printf("[TEST] Tier2 Constant Folding & Identity Simplification...\n");

	ControlFlowGraph cfg;
	BasicBlock* b0 = cfg.CreateBlock("b0");

	// 1. v1 = ADD v2, 0 -> ZExt v2
	VirtualReg v1 = cfg.AllocateVReg();
	VirtualReg v2 = cfg.AllocateVReg();
	auto inst1 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst1->SetDst(v1);
	inst1->AddOperand(Value::MakeVReg(v2));
	inst1->AddOperand(Value::MakeImmInt(0));
	auto* p_inst1 = inst1.get();
	b0->AddInstruction(std::move(inst1));

	// 2. v3 = XOR v4, v4 -> ZExt 0
	VirtualReg v3 = cfg.AllocateVReg();
	VirtualReg v4 = cfg.AllocateVReg();
	auto inst2 = std::make_unique<IRInstruction>(IROpcode::Xor);
	inst2->SetDst(v3);
	inst2->AddOperand(Value::MakeVReg(v4));
	inst2->AddOperand(Value::MakeVReg(v4));
	auto* p_inst2 = inst2.get();
	b0->AddInstruction(std::move(inst2));

	// 3. v5 = SUB 10, 3 -> ZExt 7
	VirtualReg v5 = cfg.AllocateVReg();
	auto inst3 = std::make_unique<IRInstruction>(IROpcode::Sub);
	inst3->SetDst(v5);
	inst3->AddOperand(Value::MakeImmInt(10));
	inst3->AddOperand(Value::MakeImmInt(3));
	auto* p_inst3 = inst3.get();
	b0->AddInstruction(std::move(inst3));

	// 4. v6 = MUL v7, 1 -> ZExt v7
	VirtualReg v6 = cfg.AllocateVReg();
	VirtualReg v7 = cfg.AllocateVReg();
	auto inst4 = std::make_unique<IRInstruction>(IROpcode::Mul);
	inst4->SetDst(v6);
	inst4->AddOperand(Value::MakeVReg(v7));
	inst4->AddOperand(Value::MakeImmInt(1));
	auto* p_inst4 = inst4.get();
	b0->AddInstruction(std::move(inst4));

	JitTier2Optimizer opt;
	uint32_t folded = opt.RunConstantFolding(cfg);
	Check(folded == 4, "Expected 4 folded expressions");
	Check(p_inst1->GetOpcode() == IROpcode::ZExt, "Inst1 opcode mismatch");
	Check(p_inst2->GetOpcode() == IROpcode::ZExt && p_inst2->GetOperands()[0].imm_int == 0, "Inst2 XOR fold mismatch");
	Check(p_inst3->GetOpcode() == IROpcode::ZExt && p_inst3->GetOperands()[0].imm_int == 7, "Inst3 SUB fold mismatch");

	std::printf("  [OK] Tier2 Constant Folding\n");
}

void TestGlobalValueNumbering() {
	std::printf("[TEST] Tier2 Global Value Numbering (GVN)...\n");

	ControlFlowGraph cfg;
	BasicBlock* b0 = cfg.CreateBlock("b0");

	VirtualReg v10 = cfg.AllocateVReg();
	VirtualReg v11 = cfg.AllocateVReg();

	// inst1: v1 = ADD v10, v11
	VirtualReg v1 = cfg.AllocateVReg();
	auto inst1 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst1->SetDst(v1);
	inst1->AddOperand(Value::MakeVReg(v10));
	inst1->AddOperand(Value::MakeVReg(v11));
	b0->AddInstruction(std::move(inst1));

	// inst2: v2 = ADD v10, v11 (Identical expression -> should become ZExt v1)
	VirtualReg v2 = cfg.AllocateVReg();
	auto inst2 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst2->SetDst(v2);
	inst2->AddOperand(Value::MakeVReg(v10));
	inst2->AddOperand(Value::MakeVReg(v11));
	auto* p_inst2 = inst2.get();
	b0->AddInstruction(std::move(inst2));

	JitTier2Optimizer opt;
	uint32_t gvn_count = opt.RunGlobalValueNumbering(cfg);
	Check(gvn_count == 1, "Expected 1 redundant expression eliminated");
	Check(p_inst2->GetOpcode() == IROpcode::ZExt, "GVN should convert redundant op to ZExt");
	Check(p_inst2->GetOperands()[0].vreg.id == v1.id, "GVN operand should reference v1");

	std::printf("  [OK] Tier2 Global Value Numbering\n");
}

void TestRuntimeOptimizationEngine() {
	std::printf("[TEST] Runtime Optimization Engine & Inline Cache...\n");

	RuntimeOptimizationEngine engine;
	ExecutionTier tier{};

	// 1. Hot block recording up to Tier1 promotion
	bool promoted = false;
	for (int i = 0; i < 100; ++i) {
		if (engine.RecordExecution(0x400000, tier)) {
			promoted = true;
		}
	}
	Check(promoted, "Expected block promotion after 100 executions");
	Check(tier == ExecutionTier::Tier1_OptimizedJit, "Expected Tier1 tier");

	// 2. Inline cache recording & lookup
	engine.UpdateInlineCache(0x400010, 0x500000, 0x7FFF0000);
	uint64_t target_func = 0;
	bool hit = engine.LookupInlineCache(0x400010, 0x500000, target_func);
	Check(hit, "Inline cache lookup should hit");
	Check(target_func == 0x7FFF0000, "Inline cache func ptr mismatch");

	// 3. SMC invalidation
	engine.InvalidateCodeRange(0x500000, 0x1000);
	hit = engine.LookupInlineCache(0x400010, 0x500000, target_func);
	Check(!hit, "Inline cache lookup should miss after invalidation");

	std::printf("  [OK] Runtime Optimization Engine\n");
}

} // namespace

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Tier-2 JIT Optimizations & Runtime Engine Test Suite\n");
	std::printf("================================================================================\n");

	TestConstantFolding();
	TestGlobalValueNumbering();
	TestRuntimeOptimizationEngine();

	std::printf("================================================================================\n");
	std::printf("  Results: All Tier-2 Recompiler Tests PASSED\n");
	std::printf("================================================================================\n");
	return 0;
}
