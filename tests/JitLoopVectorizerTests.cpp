// JitLoopVectorizerTests.cpp
//
// Unit tests for JIT Tier-3 Auto-SIMD Loop Vectorizer.

#include "loader/recompiler/jitLoopVectorizer.h"
#include "loader/recompiler/compilerIR.h"
#include <cstdio>
#include <cstdlib>

#define ASSERT_TRUE(cond) \
	do { \
		if (!(cond)) { \
			::printf("Assertion failed: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
			::exit(1); \
		} \
	} while (0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))

static void Test_LoopVectorizer_Coalescing() {
	::printf("[TEST] LoopVectorizer_Coalescing\n");

	Loader::Recompiler::ControlFlowGraph cfg;
	auto* bb = cfg.CreateBlock("loop_body");

	// Add 4 scalar FAdd instructions
	for (uint32_t i = 0; i < 4; ++i) {
		auto inst = std::make_unique<Loader::Recompiler::IRInstruction>(Loader::Recompiler::IROpcode::FAdd);
		inst->SetDst({100 + i, Loader::Recompiler::DataType::Float32});
		inst->AddOperand(Loader::Recompiler::Value::MakeVReg({10 + i, Loader::Recompiler::DataType::Float32}));
		inst->AddOperand(Loader::Recompiler::Value::MakeVReg({20 + i, Loader::Recompiler::DataType::Float32}));
		bb->AddInstruction(std::move(inst));
	}

	ASSERT_EQ(bb->GetInstructions().size(), 4U);

	Loader::Recompiler::JitLoopVectorizer vectorizer;
	bool modified = vectorizer.RunPass(cfg);
	ASSERT_TRUE(modified);

	// Should be collapsed into 1 vector VecAdd instruction
	ASSERT_EQ(bb->GetInstructions().size(), 1U);
	ASSERT_EQ(bb->GetInstructions()[0]->GetOpcode(), Loader::Recompiler::IROpcode::VecAdd);
	ASSERT_EQ(bb->GetInstructions()[0]->GetDst().type, Loader::Recompiler::DataType::Vec128);

	const auto& stats = vectorizer.GetStats();
	ASSERT_EQ(stats.vector_loops_found, 1U);
	ASSERT_EQ(stats.scalar_ops_coalesced, 4U);
	ASSERT_EQ(stats.vector_insts_emitted, 1U);

	::printf("  [ OK ] LoopVectorizer_Coalescing\n");
}

int main() {
	::printf("================================================================================\n");
	::printf("  KytyPS5 — JIT Loop Vectorizer & Auto-SIMD Test Suite\n");
	::printf("================================================================================\n");

	Test_LoopVectorizer_Coalescing();

	::printf("================================================================================\n");
	::printf("  Results: 1 passed, 0 failed (100%% Success Rate)\n");
	::printf("================================================================================\n");
	return 0;
}
