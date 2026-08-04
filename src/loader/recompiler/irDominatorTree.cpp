// irDominatorTree.cpp
//
// Dominator Tree, Dominance Frontiers & RPO Analysis for Target-Independent IR.

#include "loader/recompiler/irDominatorTree.h"

#include <algorithm>

namespace Loader::Recompiler {

static BasicBlock* Intersect(BasicBlock* b1, BasicBlock* b2,
                             const std::unordered_map<BasicBlock*, int>& rpo_idx,
                             const std::unordered_map<BasicBlock*, BasicBlock*>& idom) {
	BasicBlock* finger1 = b1;
	BasicBlock* finger2 = b2;

	while (finger1 != finger2) {
		while (finger1 && rpo_idx.at(finger1) > rpo_idx.at(finger2)) {
			auto it = idom.find(finger1);
			finger1 = (it != idom.end()) ? it->second : nullptr;
		}
		while (finger2 && finger1 && rpo_idx.at(finger2) > rpo_idx.at(finger1)) {
			auto it = idom.find(finger2);
			finger2 = (it != idom.end()) ? it->second : nullptr;
		}
	}
	return finger1;
}

void DominatorTree::Build(const ControlFlowGraph& cfg) {
	m_idom.clear();
	m_dom_children.clear();
	m_dom_frontier.clear();

	std::vector<BasicBlock*> rpo = cfg.ComputeReversePostOrder();
	if (rpo.empty()) return;

	BasicBlock* entry = cfg.GetEntryBlock();
	if (!entry) return;

	std::unordered_map<BasicBlock*, int> rpo_idx;
	for (size_t i = 0; i < rpo.size(); ++i) {
		rpo_idx[rpo[i]] = static_cast<int>(i);
	}

	m_idom[entry] = entry;

	bool changed = true;
	while (changed) {
		changed = false;

		for (BasicBlock* b : rpo) {
			if (b == entry) continue;

			BasicBlock* new_idom = nullptr;
			for (BasicBlock* p : b->GetPredecessors()) {
				if (m_idom.count(p)) {
					new_idom = p;
					break;
				}
			}

			if (!new_idom) continue;

			for (BasicBlock* p : b->GetPredecessors()) {
				if (p != new_idom && m_idom.count(p)) {
					new_idom = Intersect(p, new_idom, rpo_idx, m_idom);
				}
			}

			if (m_idom[b] != new_idom) {
				m_idom[b] = new_idom;
				changed = true;
			}
		}
	}

	m_idom.erase(entry); // Entry does not dominate itself as an idom child

	for (const auto& [b, idom] : m_idom) {
		m_dom_children[idom].push_back(b);
	}

	// Dominance Frontiers computation
	for (BasicBlock* b : rpo) {
		const auto& preds = b->GetPredecessors();
		if (preds.size() >= 2) {
			for (BasicBlock* p : preds) {
				BasicBlock* runner = p;
				BasicBlock* b_idom = GetIDom(b);
				while (runner && runner != b_idom && runner != b) {
					m_dom_frontier[runner].insert(b);
					runner = GetIDom(runner);
				}
			}
		}
	}
}

bool DominatorTree::Dominates(BasicBlock* dom, BasicBlock* node) const {
	if (!dom || !node) return false;
	if (dom == node) return true;

	BasicBlock* curr = node;
	while (curr) {
		BasicBlock* parent = GetIDom(curr);
		if (parent == dom) return true;
		if (parent == curr) break;
		curr = parent;
	}
	return false;
}

} // namespace Loader::Recompiler
