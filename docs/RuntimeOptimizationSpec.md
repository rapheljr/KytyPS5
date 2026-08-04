# KytyPS5 Modern Dynamic Runtime Optimization Layer Specification

## Overview
This specification details the architecture, design policies, tiered compilation pipeline, hot block detection, background recompilation thread pool, inline caching, SMC code invalidation, performance counter tracking, and optimization reporting engine for **KytyPS5**.

---

## 1. Multi-Tiered Compilation Architecture

| Tier Level | Name | Compilation Strategy | Optimization Passes | Target Latency |
| :--- | :--- | :--- | :--- | :--- |
| **Tier 0** | **Lazy Fast-JIT** | Direct x86 instruction emission | Zero optimization pass overhead | Instant (< 1 us) |
| **Tier 1** | **Optimized JIT** | Full SSA Compiler IR pipeline | 9 Optimization Passes + Linear Scan RegAlloc | Low (< 50 us) |
| **Tier 2** | **Trace JIT** | Hot trace compilation & inline cache | Trace specialization + Dead branch removal | High throughput |

### Promotion Policy & Hot Block Detection
- **Tier 0 -> Tier 1 Promotion**: Triggered automatically when a basic block's execution count reaches **100 executions**.
- **Tier 1 -> Tier 2 Trace Promotion**: Triggered when execution count reaches **1,000 executions**.
- **Asynchronous Recompilation**: Tier 1 and Tier 2 recompilation tasks are queued into a multi-threaded worker pool (`BackgroundRecompiler`), allowing guest execution to proceed without stalling on the main thread.

---

## 2. Inline Caching & Code Invalidation (SMC)

### Inline Cache Engine (`InlineCache`)
- **Structure**: Fast monomorphic/polymorphic inline cache mapping `(CallSiteRIP, TargetGuestRIP)` pairs to host function entry pointers (`HostFuncPtr`).
- **Hit/Miss Counters**: Tracks cache efficiency for virtual function dispatches and indirect jumps.

### SMC Code Invalidation (`CodeInvalidationManager`)
- **Address-Range Invalidation**: Invalidates compiled basic blocks and inline cache entries whenever guest memory write / self-modifying code (SMC) events occur within target address ranges `[start_rip, start_rip + size)`.

---

## 3. Performance Counters & Automated Report Generation

The engine maintains atomic performance counters exposed via `GenerateOptimizationReport()`:

- `tier0_executions`: Number of fast-path Tier 0 block executions.
- `tier1_promotions`: Number of blocks promoted to Tier 1 Optimized JIT.
- `tier2_trace_promotions`: Number of blocks promoted to Tier 2 Trace JIT.
- `inline_cache_hits` / `inline_cache_misses`: Efficiency metrics for indirect call/jump caching.
- `smc_code_invalidations`: Total code invalidations triggered by guest memory modifications.
- `background_compilations_queued` / `completed`: Async worker thread throughput.
- `total_compilation_time_micros`: Total CPU time spent compiling optimized blocks in microseconds.
