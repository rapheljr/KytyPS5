// JitEscapeAnalysisTests.cpp
//
// Unit & Integration Tests for JIT Escape Analysis & Scalar Replacement Pass.

#include "loader/recompiler/jitEscapeAnalysis.h"
#include "loader/recompiler/compilerIR.h"

#include <cstdio>
#include <cstdlib>

using namespace Loader::Recompiler;

static void TestStoreLoadForwarding() {
	std::printf("[TEST] EscapeAnalysis_StoreLoadForwarding\n");

	JitEscapeAnalysis sroa;

	// Create CFG with Store to stack [RSP + 16], followed by Load from [RSP + 16]
	ControlFlowGraph cfg;
	auto* bb = cfg.CreateBlock("entry");

	// 1. Store [RSP + 16], VReg(1)
	auto store_inst = std::make_unique<IRInstruction>(IROpcode::Store);
	Value mem_loc;
	mem_loc.kind = Value::Kind::MemoryRef;
	mem_loc.mem_ref.disp = 16;
	store_inst->AddOperand(mem_loc);
	store_inst->AddOperand(Value::MakeVReg({1, DataType::Int64}));
	bb->AddInstruction(std::move(store_inst));

	// 2. Load VReg(2), [RSP + 16]
	auto load_inst = std::make_unique<IRInstruction>(IROpcode::Load);
	load_inst->SetDst({2, DataType::Int64});
	load_inst->AddOperand(mem_loc);
	bb->AddInstruction(std::move(load_inst));

	bool ok = sroa.RunPass(cfg);
	if (!ok) {
		std::fprintf(stderr, "FAIL: RunPass failed to optimize stack load\n");
		std::exit(1);
	}

	const auto& stats = sroa.GetStats();
	if (stats.stack_loads_forwarded != 1 || stats.stack_slots_tracked != 1) {
		std::fprintf(stderr, "FAIL: Stats mismatch (Forwarded=%u, Tracked=%u)\n",
		             stats.stack_loads_forwarded, stats.stack_slots_tracked);
		std::exit(1);
	}

	// Verify the second instruction was transformed into Add (forwarded vreg)
	const auto& instrs = bb->GetInstructions();
	if (instrs[1]->GetOpcode() != IROpcode::Add) {
		std::fprintf(stderr, "FAIL: Load instruction not transformed into Add\n");
		std::exit(1);
	}

	std::printf("  [ OK ] EscapeAnalysis_StoreLoadForwarding\n");
}

int main() {
	std::printf("================================================================================\n");
	std::printf("  KytyPS5 — JIT Escape Analysis & SROA Test Suite\n");
	std::printf("================================================================================\n");

	TestStoreLoadForwarding();

	std::printf("================================================================================\n");
	std::printf("  Results: 1 passed, 0 failed (100%% Success Rate)\n");
	std::printf("================================================================================\n");

	return 0;
}
