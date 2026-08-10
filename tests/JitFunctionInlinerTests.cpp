// JitFunctionInlinerTests.cpp
//
// Unit & Integration Tests for JIT Function Inliner Pass.

#include "loader/recompiler/jitFunctionInliner.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static void TestInlinerCandidateRegistration() {
	std::printf("[TEST] FunctionInliner_CandidateRegistration\n");

	JitFunctionInliner inliner;

	// 1. Create a small leaf function (3 instructions)
	ControlFlowGraph small_leaf_cfg;
	auto* bb1 = small_leaf_cfg.CreateBlock("entry");

	auto add_instr = std::make_unique<IRInstruction>(IROpcode::Add);
	add_instr->SetDst({1, DataType::Int64});
	add_instr->AddOperand(Value::MakeVReg({2, DataType::Int64}));
	add_instr->AddOperand(Value::MakeImmInt(5));
	bb1->AddInstruction(std::move(add_instr));

	auto sub_instr = std::make_unique<IRInstruction>(IROpcode::Sub);
	sub_instr->SetDst({3, DataType::Int64});
	sub_instr->AddOperand(Value::MakeVReg({1, DataType::Int64}));
	sub_instr->AddOperand(Value::MakeImmInt(1));
	bb1->AddInstruction(std::move(sub_instr));

	bb1->AddInstruction(std::make_unique<IRInstruction>(IROpcode::Return));

	if (!inliner.RegisterCandidate(0x1000, small_leaf_cfg)) {
		std::fprintf(stderr, "FAIL: Failed to register small leaf function candidate\n");
		std::exit(1);
	}

	if (!inliner.IsCandidate(0x1000)) {
		std::fprintf(stderr, "FAIL: IsCandidate returned false for 0x1000\n");
		std::exit(1);
	}

	// 2. Create a non-leaf function with an indirect jump with multiple operands
	ControlFlowGraph non_leaf_cfg;
	auto* bb2 = non_leaf_cfg.CreateBlock("entry");
	auto jump_instr = std::make_unique<IRInstruction>(IROpcode::Jump);
	jump_instr->AddOperand(Value::MakeImmInt(0x9000));
	jump_instr->AddOperand(Value::MakeImmInt(0x9008)); // multiple operands flag non-leaf
	bb2->AddInstruction(std::move(jump_instr));
	bb2->AddInstruction(std::make_unique<IRInstruction>(IROpcode::Return));

	if (inliner.RegisterCandidate(0x2000, non_leaf_cfg)) {
		std::fprintf(stderr, "FAIL: Registered non-leaf function when allow_leaf_only=true\n");
		std::exit(1);
	}

	std::printf("  [ OK ] FunctionInliner_CandidateRegistration\n");
}

static void TestCallsiteInlining() {
	std::printf("[TEST] FunctionInliner_CallsiteInlining\n");

	JitFunctionInliner inliner;

	// Register small candidate at 0x4000
	ControlFlowGraph callee_cfg;
	auto* callee_bb = callee_cfg.CreateBlock("entry");
	auto callee_add = std::make_unique<IRInstruction>(IROpcode::Add);
	callee_add->SetDst({1, DataType::Int64});
	callee_add->AddOperand(Value::MakeVReg({2, DataType::Int64}));
	callee_add->AddOperand(Value::MakeImmInt(10));
	callee_bb->AddInstruction(std::move(callee_add));
	callee_bb->AddInstruction(std::make_unique<IRInstruction>(IROpcode::Return));
	inliner.RegisterCandidate(0x4000, callee_cfg);

	// Create caller CFG jumping to 0x4000
	ControlFlowGraph caller_cfg;
	auto* caller_bb = caller_cfg.CreateBlock("entry");
	auto caller_jump = std::make_unique<IRInstruction>(IROpcode::Jump);
	caller_jump->AddOperand(Value::MakeImmInt(0x4000));
	caller_bb->AddInstruction(std::move(caller_jump));

	bool inlined = inliner.RunPass(caller_cfg);
	if (!inlined) {
		std::fprintf(stderr, "FAIL: RunPass did not inline callsite\n");
		std::exit(1);
	}

	const auto& stats = inliner.GetStats();
	if (stats.callsites_inlined != 1) {
		std::fprintf(stderr, "FAIL: Callsites inlined count mismatch: %u\n", stats.callsites_inlined);
		std::exit(1);
	}

	std::printf("  [ OK ] FunctionInliner_CallsiteInlining\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — JIT Function Inliner Unit Test Suite\n");
	std::printf("================================================================================\n");

	TestInlinerCandidateRegistration();
	TestCallsiteInlining();

	std::printf("================================================================================\n");
	std::printf("  Results: 2 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
