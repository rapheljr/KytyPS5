// appleSiliconPmcProfiler.h
//
// Apple Silicon Hardware Performance Monitor Counter (PMC) Profiler for KytyPS5 JIT.
// Measures Instructions Per Cycle (IPC), Branch Mispredicts, and L1 I-Cache misses on M-series cores.

#ifndef LOADER_RECOMPILER_APPLE_SILICON_PMC_PROFILER_H
#define LOADER_RECOMPILER_APPLE_SILICON_PMC_PROFILER_H

#include "common/common.h"

#include <cstdint>
#include <string>

namespace Loader::Recompiler {

struct PmcSnapshot {
	uint64_t instructions_retired = 0;
	uint64_t cpu_cycles           = 0;
	uint64_t branch_mispredicts   = 0;
	uint64_t l1i_cache_misses     = 0;
	double   ipc                  = 0.0;
	double   branch_miss_rate_pct = 0.0;
};

class AppleSiliconPmcProfiler {
public:
	AppleSiliconPmcProfiler();
	~AppleSiliconPmcProfiler() = default;

	KYTY_CLASS_NO_COPY(AppleSiliconPmcProfiler);

	bool StartSession();
	void RecordSample(uint64_t instrs, uint64_t cycles, uint64_t branch_misses, uint64_t l1i_misses);
	PmcSnapshot StopSession();

	[[nodiscard]] const PmcSnapshot& GetCurrentMetrics() const noexcept { return m_snapshot; }
	[[nodiscard]] bool IsActive() const noexcept { return m_is_active; }

private:
	PmcSnapshot m_snapshot{};
	bool        m_is_active = false;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_APPLE_SILICON_PMC_PROFILER_H
