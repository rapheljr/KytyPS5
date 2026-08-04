// irDominatorTree.h
//
// Dominator Tree, Dominance Frontiers & RPO Analysis for Target-Independent IR.

#ifndef LOADER_RECOMPILER_IR_DOMINATOR_TREE_H
#define LOADER_RECOMPILER_IR_DOMINATOR_TREE_H

#include "loader/recompiler/compilerIR.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Loader::Recompiler {

class DominatorTree {
public:
	DominatorTree() = default;
	~DominatorTree() = default;

	KYTY_CLASS_NO_COPY(DominatorTree);

	void Build(const ControlFlowGraph& cfg);

	[[nodiscard]] BasicBlock* GetIDom(BasicBlock* block) const {
		auto it = m_idom.find(block);
		return (it != m_idom.end()) ? it->second : nullptr;
	}

	[[nodiscard]] bool Dominates(BasicBlock* dom, BasicBlock* node) const;

	[[nodiscard]] const std::unordered_set<BasicBlock*>& GetDominanceFrontier(BasicBlock* block) const {
		static const std::unordered_set<BasicBlock*> empty_set;
		auto it = m_dom_frontier.find(block);
		return (it != m_dom_frontier.end()) ? it->second : empty_set;
	}

private:
	std::unordered_map<BasicBlock*, BasicBlock*>                  m_idom;
	std::unordered_map<BasicBlock*, std::vector<BasicBlock*>>     m_dom_children;
	std::unordered_map<BasicBlock*, std::unordered_set<BasicBlock*>> m_dom_frontier;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_IR_DOMINATOR_TREE_H
