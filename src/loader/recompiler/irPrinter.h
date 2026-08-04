// irPrinter.h
//
// IR Verifier, Human-Readable Printer & Graphviz (.dot) Exporter for Target-Independent IR.

#ifndef LOADER_RECOMPILER_IR_PRINTER_H
#define LOADER_RECOMPILER_IR_PRINTER_H

#include "loader/recompiler/compilerIR.h"
#include "loader/recompiler/irDominatorTree.h"

#include <string>
#include <vector>

namespace Loader::Recompiler {

class IRVerifier {
public:
	IRVerifier() = default;
	~IRVerifier() = default;

	struct VerificationResult {
		bool valid = true;
		std::vector<std::string> errors;
	};

	static VerificationResult Verify(const ControlFlowGraph& cfg, const DominatorTree& dom_tree);
};

class IRPrinter {
public:
	IRPrinter() = default;
	~IRPrinter() = default;

	static std::string Print(const ControlFlowGraph& cfg);
	static std::string PrintBlock(const BasicBlock& block);
	static std::string PrintInstruction(const IRInstruction& inst);
};

class GraphvizExporter {
public:
	GraphvizExporter() = default;
	~GraphvizExporter() = default;

	static std::string ExportCFG(const ControlFlowGraph& cfg);
	static std::string ExportDominatorTree(const ControlFlowGraph& cfg, const DominatorTree& dom_tree);
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_IR_PRINTER_H
