#pragma once

#include "loader/recompiler/compilerIR.h"
#include <cstdint>
#include <vector>

namespace Loader::Recompiler {

struct LoopVectorizerStats {
	uint32_t vector_loops_found = 0;
	uint32_t scalar_ops_coalesced = 0;
	uint32_t vector_insts_emitted = 0;
};

class JitLoopVectorizer {
public:
	JitLoopVectorizer() = default;
	~JitLoopVectorizer() = default;

	// Runs the Auto-SIMD Loop Vectorization pass on the given CFG
	bool RunPass(ControlFlowGraph& cfg);

	const LoopVectorizerStats& GetStats() const { return m_stats; }
	void ResetStats() { m_stats = LoopVectorizerStats{}; }

private:
	bool VectorizeBasicBlock(BasicBlock* bb);

	LoopVectorizerStats m_stats;
};

} // namespace Loader::Recompiler
