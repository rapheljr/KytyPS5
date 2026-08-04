// moduleDependencyGraph.cpp
//
// Dynamic Module Dependency Graph & Topological Sorting Implementation.

#include "loader/moduleDependencyGraph.h"

#include <algorithm>
#include <sstream>

namespace Loader {

void ModuleDependencyGraph::Clear() {
	m_nodes.clear();
}

bool ModuleDependencyGraph::AddModule(const std::string& name, Program* program, const std::vector<std::string>& dependencies) {
	if (name.empty()) return false;
	ModuleNode node{};
	node.name         = name;
	node.program      = program;
	node.dependencies = dependencies;
	m_nodes[name]     = node;
	return true;
}

bool ModuleDependencyGraph::BuildGraph() {
	for (auto& [name, node] : m_nodes) {
		for (const auto& dep : node.dependencies) {
			auto it = m_nodes.find(dep);
			if (it != m_nodes.end()) {
				it->second.dependents.push_back(name);
			}
		}
	}
	return true;
}

bool ModuleDependencyGraph::VisitTopological(const std::string& name, std::unordered_set<std::string>& visited,
                                             std::unordered_set<std::string>& in_stack, std::vector<Program*>& order) {
	if (in_stack.find(name) != in_stack.end()) {
		return false; // Cycle detected
	}
	if (visited.find(name) != visited.end()) {
		return true; // Already processed
	}

	visited.insert(name);
	in_stack.insert(name);

	auto it = m_nodes.find(name);
	if (it != m_nodes.end()) {
		for (const auto& dep : it->second.dependencies) {
			if (!VisitTopological(dep, visited, in_stack, order)) {
				return false;
			}
		}
		if (it->second.program) {
			order.push_back(it->second.program);
		}
	}

	in_stack.erase(name);
	return true;
}

bool ModuleDependencyGraph::DetectCycles() {
	std::unordered_set<std::string> visited;
	std::unordered_set<std::string> in_stack;
	std::vector<Program*> dummy_order;

	for (const auto& [name, node] : m_nodes) {
		if (visited.find(name) == visited.end()) {
			if (!VisitTopological(name, visited, in_stack, dummy_order)) {
				return true; // Cycle detected
			}
		}
	}
	return false;
}

std::vector<Program*> ModuleDependencyGraph::GetInitializationOrder() {
	std::unordered_set<std::string> visited;
	std::unordered_set<std::string> in_stack;
	std::vector<Program*> order;

	for (const auto& [name, node] : m_nodes) {
		if (visited.find(name) == visited.end()) {
			VisitTopological(name, visited, in_stack, order);
		}
	}
	return order;
}

std::string ModuleDependencyGraph::GetDiagnostics() const {
	std::stringstream ss;
	ss << "=== Module Dependency Graph Topology (" << m_nodes.size() << " modules) ===\n";
	for (const auto& [name, node] : m_nodes) {
		ss << "Module: " << name << " -> [";
		for (size_t i = 0; i < node.dependencies.size(); ++i) {
			ss << node.dependencies[i] << (i + 1 < node.dependencies.size() ? ", " : "");
		}
		ss << "]\n";
	}
	return ss.str();
}

} // namespace Loader
