// moduleDependencyGraph.h
//
// Dynamic Module Dependency Graph & Topological Sorting for PS5 Game Loader.

#ifndef LOADER_MODULE_DEPENDENCY_GRAPH_H
#define LOADER_MODULE_DEPENDENCY_GRAPH_H

#include "common/common.h"
#include "loader/runtimeLinker.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Loader {

struct ModuleNode {
	std::string              name;
	Program*                 program = nullptr;
	std::vector<std::string> dependencies;
	std::vector<std::string> dependents;
	bool                     visited     = false;
	bool                     in_stack    = false;
	bool                     initialized = false;
};

class ModuleDependencyGraph {
public:
	ModuleDependencyGraph() = default;
	~ModuleDependencyGraph() = default;

	KYTY_CLASS_NO_COPY(ModuleDependencyGraph);

	void Clear();
	bool AddModule(const std::string& name, Program* program, const std::vector<std::string>& dependencies);
	bool BuildGraph();

	bool DetectCycles();
	[[nodiscard]] std::vector<Program*> GetInitializationOrder();
	[[nodiscard]] std::string GetDiagnostics() const;

	[[nodiscard]] size_t GetModuleCount() const { return m_nodes.size(); }

private:
	bool VisitTopological(const std::string& name, std::unordered_set<std::string>& visited,
	                      std::unordered_set<std::string>& in_stack, std::vector<Program*>& order);

	std::unordered_map<std::string, ModuleNode> m_nodes;
};

} // namespace Loader

#endif // LOADER_MODULE_DEPENDENCY_GRAPH_H
