// compilerIR.cpp
//
// Target-Independent Compiler Intermediate Representation (IR) & Control Flow Graph (CFG).

#include "loader/recompiler/compilerIR.h"

#include <algorithm>
#include <unordered_set>

namespace Loader::Recompiler {

static void RpoDfs(BasicBlock* block, std::unordered_set<BasicBlock*>& visited, std::vector<BasicBlock*>& post_order) {
	if (!block || visited.count(block)) return;

	visited.insert(block);

	for (BasicBlock* succ : block->GetSuccessors()) {
		RpoDfs(succ, visited, post_order);
	}

	post_order.push_back(block);
}

std::vector<BasicBlock*> ControlFlowGraph::ComputeReversePostOrder() const {
	std::vector<BasicBlock*> post_order;
	std::unordered_set<BasicBlock*> visited;

	if (m_entry_block) {
		RpoDfs(m_entry_block, visited, post_order);
	}

	std::reverse(post_order.begin(), post_order.end());
	return post_order;
}

} // namespace Loader::Recompiler
