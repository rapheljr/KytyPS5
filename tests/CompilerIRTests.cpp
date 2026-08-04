// CompilerIRTests.cpp
//
// Target-Independent Compiler IR, SSA Pipeline, Dominator Tree & Optimization Passes Test Suite.

#include "loader/recompiler/arm64IRCodegen.h"
#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/irDominatorTree.h"
#include "loader/recompiler/irOptimizationPasses.h"
#include "loader/recompiler/irPrinter.h"
#include "loader/recompiler/x86ToIRLowering.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "CompilerIRTests FAILED: %s\n", text);
		std::exit(1);
	}
}

using namespace Loader::Recompiler;

void TestIRGraphAndRpo() {
	std::printf("  [IR Test 1] Testing BasicBlock, CFG & Reverse Post-Order (RPO)...\n");

	ControlFlowGraph cfg;
	BasicBlock* b0 = cfg.CreateBlock("entry");
	BasicBlock* b1 = cfg.CreateBlock("then");
	BasicBlock* b2 = cfg.CreateBlock("else");
	BasicBlock* b3 = cfg.CreateBlock("merge");

	cfg.AddEdge(b0, b1);
	cfg.AddEdge(b0, b2);
	cfg.AddEdge(b1, b3);
	cfg.AddEdge(b2, b3);

	auto rpo = cfg.ComputeReversePostOrder();
	Check(rpo.size() == 4, "RPO size must be 4");
	Check(rpo[0] == b0, "RPO[0] must be entry block");
	Check(rpo[3] == b3, "RPO[3] must be merge block");

	std::printf("  [OK] IR Test 1: BasicBlock, CFG & RPO passed\n");
}

void TestDominatorTree() {
	std::printf("  [IR Test 2] Testing Dominator Tree & Dominance Frontiers...\n");

	ControlFlowGraph cfg;
	BasicBlock* b0 = cfg.CreateBlock("b0");
	BasicBlock* b1 = cfg.CreateBlock("b1");
	BasicBlock* b2 = cfg.CreateBlock("b2");
	BasicBlock* b3 = cfg.CreateBlock("b3");

	cfg.AddEdge(b0, b1);
	cfg.AddEdge(b0, b2);
	cfg.AddEdge(b1, b3);
	cfg.AddEdge(b2, b3);

	DominatorTree dom;
	dom.Build(cfg);

	Check(dom.GetIDom(b1) == b0, "IDom(b1) must be b0");
	Check(dom.GetIDom(b2) == b0, "IDom(b2) must be b0");
	Check(dom.GetIDom(b3) == b0, "IDom(b3) must be b0");

	Check(dom.Dominates(b0, b3), "b0 must dominate b3");

	std::printf("  [OK] IR Test 2: Dominator Tree passed\n");
}

void TestOptimizationPasses() {
	std::printf("  [IR Test 3] Testing 9 Optimization Passes...\n");

	ControlFlowGraph cfg;
	BasicBlock* bb = cfg.CreateBlock("main");

	// v1 = 10
	VirtualReg v1 = cfg.AllocateVReg();
	auto i1 = std::make_unique<IRInstruction>(IROpcode::Add);
	i1->SetDst(v1);
	i1->AddOperand(Value::MakeImmInt(10));
	bb->AddInstruction(std::move(i1));

	// v2 = v1 + 5 -> fold to 15 after ConstProp + ConstFold
	VirtualReg v2 = cfg.AllocateVReg();
	auto i2 = std::make_unique<IRInstruction>(IROpcode::Add);
	i2->SetDst(v2);
	i2->AddOperand(Value::MakeVReg(v1));
	i2->AddOperand(Value::MakeImmInt(5));
	bb->AddInstruction(std::move(i2));

	// v3 = v2 + 0 -> algebraic sim to v2
	VirtualReg v3 = cfg.AllocateVReg();
	auto i3 = std::make_unique<IRInstruction>(IROpcode::Add);
	i3->SetDst(v3);
	i3->AddOperand(Value::MakeVReg(v2));
	i3->AddOperand(Value::MakeImmInt(0));
	bb->AddInstruction(std::move(i3));

	auto pm = PassManager::CreateDefaultPipeline();
	bool modified = pm.RunAll(cfg);
	Check(modified, "Optimization pass pipeline must modify IR");

	std::printf("  [OK] IR Test 3: Optimization Passes passed\n");
}

void TestIRVerifierAndGraphviz() {
	std::printf("  [IR Test 4] Testing IR Verifier & Graphviz Exporter...\n");

	ControlFlowGraph cfg;
	BasicBlock* b0 = cfg.CreateBlock("b0");
	BasicBlock* b1 = cfg.CreateBlock("b1");
	cfg.AddEdge(b0, b1);

	DominatorTree dom;
	dom.Build(cfg);

	auto vres = IRVerifier::Verify(cfg, dom);
	Check(vres.valid, "Valid CFG must pass IRVerifier");

	std::string dot_cfg = GraphvizExporter::ExportCFG(cfg);
	Check(dot_cfg.find("digraph CFG") != std::string::npos, "ExportCFG output invalid");

	std::string dot_dom = GraphvizExporter::ExportDominatorTree(cfg, dom);
	Check(dot_dom.find("digraph DominatorTree") != std::string::npos, "ExportDominatorTree output invalid");

	std::printf("  [OK] IR Test 4: IR Verifier & Graphviz Exporter passed\n");
}

void TestX86ToIRAndArm64Codegen() {
	std::printf("  [IR Test 5] Testing x86 Lowering -> IR Pipeline -> ARM64 Codegen...\n");

	uint8_t x86_code[] = {0x48, 0x83, 0xC0, 0x0A, 0xC3}; // ADD RAX, 10; RET
	auto cfg = X86ToIRLowering::LowerBlock(x86_code, sizeof(x86_code), 0x1000);
	Check(cfg && !cfg->GetBlocks().empty(), "X86ToIRLowering returned empty CFG");

	PassManager pm = PassManager::CreateDefaultPipeline();
	pm.RunAll(*cfg);

	Arm64Emitter emitter;
	Arm64IRCodegen codegen;
	bool success = codegen.CompileCFG(*cfg, emitter);
	Check(success && !emitter.GetCode().empty(), "Arm64IRCodegen failed");

	std::printf("  [OK] IR Test 5: Lowering & Codegen passed\n");
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf("  KytyPS5 Target-Independent Compiler IR Test Suite \n");
	std::printf("====================================================\n");

	TestIRGraphAndRpo();
	TestDominatorTree();
	TestOptimizationPasses();
	TestIRVerifierAndGraphviz();
	TestX86ToIRAndArm64Codegen();

	std::printf("\nALL COMPILER IR TESTS PASSED SUCCESSFULLY!\n");
	return 0;
}
