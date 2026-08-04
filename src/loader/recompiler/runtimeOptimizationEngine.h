// runtimeOptimizationEngine.h
//
// Modern Dynamic Runtime Optimization Layer for KytyPS5 ARM64 Recompiler.

#ifndef LOADER_RECOMPILER_RUNTIME_OPTIMIZATION_ENGINE_H
#define LOADER_RECOMPILER_RUNTIME_OPTIMIZATION_ENGINE_H

#include "common/common.h"
#include "loader/recompiler/x86BlockCache.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

enum class ExecutionTier : uint8_t {
	Tier0_LazyFastJit = 0,
	Tier1_OptimizedJit,
	Tier2_TraceJit
};

struct HotBlockMetadata {
	uint64_t                guest_rip      = 0;
	std::atomic<uint64_t>   execution_count{0};
	std::atomic<ExecutionTier> current_tier{ExecutionTier::Tier0_LazyFastJit};
	uint32_t                hot_threshold  = 100; // Trigger promotion after 100 executions
};

struct InlineCacheEntry {
	uint64_t call_site_rip   = 0;
	uint64_t target_guest_rip = 0;
	uint64_t host_func_ptr   = 0;
	std::atomic<uint64_t> hit_count{0};
};

struct RuntimePerfCounters {
	std::atomic<uint64_t> tier0_executions{0};
	std::atomic<uint64_t> tier1_promotions{0};
	std::atomic<uint64_t> tier2_trace_promotions{0};
	std::atomic<uint64_t> inline_cache_hits{0};
	std::atomic<uint64_t> inline_cache_misses{0};
	std::atomic<uint64_t> smc_code_invalidations{0};
	std::atomic<uint64_t> background_compilations_queued{0};
	std::atomic<uint64_t> background_compilations_completed{0};
	std::atomic<uint64_t> total_compilation_time_micros{0};
};

class BackgroundRecompiler {
public:
	explicit BackgroundRecompiler(size_t worker_threads = 2);
	~BackgroundRecompiler();

	KYTY_CLASS_NO_COPY(BackgroundRecompiler);

	void QueueTask(std::function<void()> task);
	void Stop();

private:
	void WorkerLoop();

	std::vector<std::thread> m_workers;
	std::queue<std::function<void()>> m_tasks;
	std::mutex               m_mutex;
	std::condition_variable  m_cv;
	std::atomic<bool>        m_stopping{false};
};

class RuntimeOptimizationEngine {
public:
	RuntimeOptimizationEngine();
	~RuntimeOptimizationEngine();

	KYTY_CLASS_NO_COPY(RuntimeOptimizationEngine);

	// 1. Hot Block Detection & Tiered Compilation
	bool RecordExecution(uint64_t guest_rip, ExecutionTier& out_new_tier);
	void PromoteBlock(uint64_t guest_rip, ExecutionTier target_tier);

	// 2. Inline Caching
	bool LookupInlineCache(uint64_t call_site_rip, uint64_t guest_target_rip, uint64_t& out_host_func);
	void UpdateInlineCache(uint64_t call_site_rip, uint64_t guest_target_rip, uint64_t host_func);

	// 3. Code Invalidation (SMC)
	void InvalidateCodeRange(uint64_t start_rip, size_t size_bytes);

	// 4. Background Recompilation
	void ScheduleBackgroundCompilation(std::function<void()> compile_task);

	// 5. Performance Counters & Reports
	RuntimePerfCounters& GetCounters() noexcept { return m_counters; }
	[[nodiscard]] const RuntimePerfCounters& GetCounters() const noexcept { return m_counters; }
	std::string GenerateOptimizationReport() const;

private:
	std::unordered_map<uint64_t, HotBlockMetadata> m_hot_blocks;
	std::unordered_map<uint64_t, InlineCacheEntry> m_inline_caches;
	mutable std::mutex                              m_engine_mutex;
	BackgroundRecompiler                            m_background_recompiler;
	RuntimePerfCounters                             m_counters;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_RUNTIME_OPTIMIZATION_ENGINE_H
