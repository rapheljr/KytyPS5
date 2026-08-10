// JitTier2OptimizerTests.cpp
//
// Unit & Integration Tests for Tier-2 Profile-Guided JIT Optimizer (GVN & CF).

#include "loader/recompiler/jitTier2Optimizer.h"

#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static void TestConstantFoldingAndAlgebraicSimplification() {
	std::printf("[TEST] Tier2_ConstantFolding\n");

	JitTier2Optimizer optimizer;
	ControlFlowGraph cfg;

	BasicBlock* bb = cfg.CreateBlock("entry");
	VirtualReg r0 = cfg.AllocateVReg();
	VirtualReg r1 = cfg.AllocateVReg();

	// 1. ADD r0, r1, 0 -> ZExt r0, r1
	auto inst1 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst1->SetDst(r0);
	inst1->AddOperand(Value::MakeVReg(r1));
	inst1->AddOperand(Value::MakeImmInt(0));
	bb->AddInstruction(std::move(inst1));

	// 2. XOR r2, r1, r1 -> ZExt r2, 0
	VirtualReg r2 = cfg.AllocateVReg();
	auto inst2 = std::make_unique<IRInstruction>(IROpcode::Xor);
	inst2->SetDst(r2);
	inst2->AddOperand(Value::MakeVReg(r1));
	inst2->AddOperand(Value::MakeVReg(r1));
	bb->AddInstruction(std::move(inst2));

	uint32_t folded = optimizer.RunConstantFolding(cfg);
	if (folded != 2) {
		std::fprintf(stderr, "FAIL: Expected 2 folded instructions, got %u\n", folded);
		std::exit(1);
	}

	const auto& instructions = bb->GetInstructions();
	if (instructions[0]->GetOpcode() != IROpcode::ZExt ||
	    instructions[1]->GetOpcode() != IROpcode::ZExt) {
		std::fprintf(stderr, "FAIL: Simplified opcodes mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] Tier2_ConstantFolding\n");
}

static void TestGlobalValueNumbering() {
	std::printf("[TEST] Tier2_GlobalValueNumbering\n");

	JitTier2Optimizer optimizer;
	ControlFlowGraph cfg;

	BasicBlock* bb = cfg.CreateBlock("entry");
	VirtualReg r0 = cfg.AllocateVReg();
	VirtualReg r1 = cfg.AllocateVReg();
	VirtualReg r2 = cfg.AllocateVReg();
	VirtualReg r3 = cfg.AllocateVReg();

	// Compute r0 = r1 + r2
	auto inst1 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst1->SetDst(r0);
	inst1->AddOperand(Value::MakeVReg(r1));
	inst1->AddOperand(Value::MakeVReg(r2));
	bb->AddInstruction(std::move(inst1));

	// Duplicate compute r3 = r1 + r2 (should be replaced with ZExt r3, r0)
	auto inst2 = std::make_unique<IRInstruction>(IROpcode::Add);
	inst2->SetDst(r3);
	inst2->AddOperand(Value::MakeVReg(r1));
	inst2->AddOperand(Value::MakeVReg(r2));
	bb->AddInstruction(std::move(inst2));

	uint32_t gvn = optimizer.RunGlobalValueNumbering(cfg);
	if (gvn != 1) {
		std::fprintf(stderr, "FAIL: Expected 1 GVN elimination, got %u\n", gvn);
		std::exit(1);
	}

	const auto& instructions = bb->GetInstructions();
	if (instructions[1]->GetOpcode() != IROpcode::ZExt ||
	    instructions[1]->GetOperands()[0].vreg.id != r0.id) {
		std::fprintf(stderr, "FAIL: GVN replacement mismatch\n");
		std::exit(1);
	}

	std::printf("  [ OK ] Tier2_GlobalValueNumbering\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — Tier-2 Profile-Guided JIT Optimizer Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestConstantFoldingAndAlgebraicSimplification();
	TestGlobalValueNumbering();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
