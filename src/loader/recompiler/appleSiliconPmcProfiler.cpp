// appleSiliconPmcProfiler.cpp
//
// Apple Silicon Hardware Performance Monitor Counter (PMC) Profiler Implementation.

#include "loader/recompiler/appleSiliconPmcProfiler.h"

namespace Loader::Recompiler {

AppleSiliconPmcProfiler::AppleSiliconPmcProfiler() = default;

bool AppleSiliconPmcProfiler::StartSession() {
	m_snapshot = {};
	m_is_active = true;
	return true;
}

void AppleSiliconPmcProfiler::RecordSample(uint64_t instrs, uint64_t cycles, uint64_t branch_misses, uint64_t l1i_misses) {
	if (!m_is_active) return;

	m_snapshot.instructions_retired += instrs;
	m_snapshot.cpu_cycles           += cycles;
	m_snapshot.branch_mispredicts   += branch_misses;
	m_snapshot.l1i_cache_misses     += l1i_misses;

	if (m_snapshot.cpu_cycles > 0) {
		m_snapshot.ipc = static_cast<double>(m_snapshot.instructions_retired) / static_cast<double>(m_snapshot.cpu_cycles);
	}

	if (m_snapshot.instructions_retired > 0) {
		m_snapshot.branch_miss_rate_pct = (static_cast<double>(m_snapshot.branch_mispredicts) / static_cast<double>(m_snapshot.instructions_retired)) * 100.0;
	}
}

PmcSnapshot AppleSiliconPmcProfiler::StopSession() {
	m_is_active = false;
	return m_snapshot;
}

} // namespace Loader::Recompiler
