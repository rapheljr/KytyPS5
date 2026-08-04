// runtimeOptimizationEngine.cpp
//
// Modern Dynamic Runtime Optimization Layer for KytyPS5 ARM64 Recompiler.

#include "loader/recompiler/runtimeOptimizationEngine.h"

#include <chrono>
#include <sstream>

namespace Loader::Recompiler {

// ─── BackgroundRecompiler ───────────────────────────────────────────────────

BackgroundRecompiler::BackgroundRecompiler(size_t worker_threads) {
	for (size_t i = 0; i < worker_threads; ++i) {
		m_workers.emplace_back(&BackgroundRecompiler::WorkerLoop, this);
	}
}

BackgroundRecompiler::~BackgroundRecompiler() {
	Stop();
}

void BackgroundRecompiler::Stop() {
	if (m_stopping.exchange(true)) return;

	m_cv.notify_all();
	for (auto& worker : m_workers) {
		if (worker.joinable()) {
			worker.join();
		}
	}
	m_workers.clear();
}

void BackgroundRecompiler::QueueTask(std::function<void()> task) {
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_tasks.push(std::move(task));
	}
	m_cv.notify_one();
}

void BackgroundRecompiler::WorkerLoop() {
	while (!m_stopping) {
		std::function<void()> task;
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_cv.wait(lock, [this] { return m_stopping || !m_tasks.empty(); });
			if (m_stopping && m_tasks.empty()) break;

			task = std::move(m_tasks.front());
			m_tasks.pop();
		}
		if (task) {
			task();
		}
	}
}

// ─── RuntimeOptimizationEngine ─────────────────────────────────────────────

RuntimeOptimizationEngine::RuntimeOptimizationEngine()
    : m_background_recompiler(2) {}

RuntimeOptimizationEngine::~RuntimeOptimizationEngine() = default;

bool RuntimeOptimizationEngine::RecordExecution(uint64_t guest_rip, ExecutionTier& out_new_tier) {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	m_counters.tier0_executions++;

	auto& meta = m_hot_blocks[guest_rip];
	meta.guest_rip = guest_rip;
	uint64_t count = ++meta.execution_count;

	ExecutionTier current = meta.current_tier.load();
	if (current == ExecutionTier::Tier0_LazyFastJit && count >= meta.hot_threshold) {
		meta.current_tier.store(ExecutionTier::Tier1_OptimizedJit);
		out_new_tier = ExecutionTier::Tier1_OptimizedJit;
		m_counters.tier1_promotions++;
		return true; // Promotion triggered!
	} else if (current == ExecutionTier::Tier1_OptimizedJit && count >= meta.hot_threshold * 10) {
		meta.current_tier.store(ExecutionTier::Tier2_TraceJit);
		out_new_tier = ExecutionTier::Tier2_TraceJit;
		m_counters.tier2_trace_promotions++;
		return true; // Trace JIT Promotion triggered!
	}

	out_new_tier = current;
	return false;
}

void RuntimeOptimizationEngine::PromoteBlock(uint64_t guest_rip, ExecutionTier target_tier) {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	auto& meta = m_hot_blocks[guest_rip];
	meta.current_tier.store(target_tier);
	if (target_tier == ExecutionTier::Tier1_OptimizedJit) m_counters.tier1_promotions++;
	else if (target_tier == ExecutionTier::Tier2_TraceJit) m_counters.tier2_trace_promotions++;
}

bool RuntimeOptimizationEngine::LookupInlineCache(uint64_t call_site_rip, uint64_t guest_target_rip, uint64_t& out_host_func) {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	auto it = m_inline_caches.find(call_site_rip);
	if (it != m_inline_caches.end() && it->second.target_guest_rip == guest_target_rip) {
		it->second.hit_count++;
		m_counters.inline_cache_hits++;
		out_host_func = it->second.host_func_ptr;
		return true;
	}
	m_counters.inline_cache_misses++;
	return false;
}

void RuntimeOptimizationEngine::UpdateInlineCache(uint64_t call_site_rip, uint64_t guest_target_rip, uint64_t host_func) {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	auto& entry = m_inline_caches[call_site_rip];
	entry.call_site_rip = call_site_rip;
	entry.target_guest_rip = guest_target_rip;
	entry.host_func_ptr = host_func;
}

void RuntimeOptimizationEngine::InvalidateCodeRange(uint64_t start_rip, size_t size_bytes) {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	uint64_t end_rip = start_rip + size_bytes;

	for (auto it = m_hot_blocks.begin(); it != m_hot_blocks.end();) {
		if (it->first >= start_rip && it->first < end_rip) {
			it = m_hot_blocks.erase(it);
			m_counters.smc_code_invalidations++;
		} else {
			++it;
		}
	}

	for (auto it = m_inline_caches.begin(); it != m_inline_caches.end();) {
		if (it->second.target_guest_rip >= start_rip && it->second.target_guest_rip < end_rip) {
			it = m_inline_caches.erase(it);
		} else {
			++it;
		}
	}
}

void RuntimeOptimizationEngine::ScheduleBackgroundCompilation(std::function<void()> compile_task) {
	m_counters.background_compilations_queued++;
	m_background_recompiler.QueueTask([this, task = std::move(compile_task)]() {
		auto start = std::chrono::high_resolution_clock::now();
		task();
		auto end = std::chrono::high_resolution_clock::now();
		uint64_t duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		m_counters.total_compilation_time_micros += duration;
		m_counters.background_compilations_completed++;
	});
}

std::string RuntimeOptimizationEngine::GenerateOptimizationReport() const {
	std::lock_guard<std::mutex> lock(m_engine_mutex);
	std::stringstream ss;

	ss << "# KytyPS5 Runtime Optimization Engine Report\n\n";
	ss << "## Performance Counters\n";
	ss << "- **Tier 0 Fast-JIT Executions**: " << m_counters.tier0_executions.load() << "\n";
	ss << "- **Tier 1 JIT Promotions**: " << m_counters.tier1_promotions.load() << "\n";
	ss << "- **Tier 2 Trace JIT Promotions**: " << m_counters.tier2_trace_promotions.load() << "\n";
	ss << "- **Inline Cache Hits**: " << m_counters.inline_cache_hits.load() << "\n";
	ss << "- **Inline Cache Misses**: " << m_counters.inline_cache_misses.load() << "\n";
	ss << "- **SMC Code Invalidations**: " << m_counters.smc_code_invalidations.load() << "\n";
	ss << "- **Background Compilations Queued**: " << m_counters.background_compilations_queued.load() << "\n";
	ss << "- **Background Compilations Completed**: " << m_counters.background_compilations_completed.load() << "\n";
	ss << "- **Total JIT Compilation Time**: " << m_counters.total_compilation_time_micros.load() << " us\n\n";

	ss << "## Tracked Hot Blocks (" << m_hot_blocks.size() << " total)\n";
	for (const auto& [rip, meta] : m_hot_blocks) {
		const char* tier_str = (meta.current_tier == ExecutionTier::Tier2_TraceJit) ? "Tier 2 (Trace)" :
		                       (meta.current_tier == ExecutionTier::Tier1_OptimizedJit) ? "Tier 1 (Optimized)" : "Tier 0 (Fast)";
		ss << "- `0x" << std::hex << rip << std::dec << "`: " << meta.execution_count.load()
		   << " executions [" << tier_str << "]\n";
	}

	return ss.str();
}

} // namespace Loader::Recompiler
