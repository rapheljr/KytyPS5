// ps5JitDispatchLoop.cpp
//
// PS5 ARM64 JIT Dispatch Loop Implementation.

#include "loader/ps5JitDispatchLoop.h"

#include "common/logging/log.h"

#include <chrono>
#include <cstring>
#include <stdexcept>

#if defined(__aarch64__)
#include <sys/time.h>
static inline uint64_t ReadHostCycles() {
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}
#else
#include <chrono>
static inline uint64_t ReadHostCycles() {
    // Fallback: use nanoseconds as proxy for cycle estimate
    return static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
}
#endif

namespace Loader {

Ps5JitDispatchLoop::Ps5JitDispatchLoop(Recompiler::X86RuntimeBridge& bridge,
                                        Recompiler::JitTelemetryCollector& telemetry)
    : m_bridge(bridge), m_telemetry(telemetry) {
    std::memset(&m_ctx, 0, sizeof(m_ctx));
    // Initialise rflags to valid x86 state (bit 1 always set)
    m_ctx.rflags = 0x02;
}

void Ps5JitDispatchLoop::SetupFromLoadResult(const OpenOrbisLoadResult& result) {
    std::memset(&m_ctx, 0, sizeof(m_ctx));
    m_ctx.rflags   = 0x02;
    m_ctx.rip      = result.entry_vaddr;

    // Guest stack: place at top of image + 8 MiB guard region
    // (actual host stack backing provided by the caller's thread)
    m_ctx.rsp = result.base_vaddr + result.image_size + (8ULL * 1024 * 1024);

    m_image_base = result.image_buffer.data();
    m_image_size = result.image_buffer.size();
    m_base_vaddr = result.base_vaddr;

    m_stop_requested.store(false, std::memory_order_release);

    LOGF("[Ps5JitDispatchLoop] Ready: entry=0x%llx base=0x%llx sz=0x%llx\n",
                  (unsigned long long)result.entry_vaddr,
                  (unsigned long long)result.base_vaddr,
                  (unsigned long long)result.image_size);
}

void Ps5JitDispatchLoop::Reset(uint64_t entry_vaddr) {
    m_ctx.rip = entry_vaddr;
    m_stop_requested.store(false, std::memory_order_release);
}

// ─── One frame slice ──────────────────────────────────────────────────────────

DispatchResult Ps5JitDispatchLoop::RunSlice() {
    DispatchResult result;

    if (m_stop_requested.load(std::memory_order_acquire)) {
        result.stop_reason = DispatchStopReason::ExternalStop;
        result.guest_rip   = m_ctx.rip;
        return result;
    }

    if (m_image_base == nullptr) {
        result.stop_reason = DispatchStopReason::Exception;
        result.error_msg   = "No guest image loaded";
        return result;
    }

    const auto slice_start = std::chrono::steady_clock::now();
    uint32_t   blocks_run  = 0;
    uint64_t   cycles_acc  = 0;

    while (blocks_run < m_config.max_blocks_per_slice) {
        // Check external stop request
        if (m_stop_requested.load(std::memory_order_acquire)) {
            result.stop_reason = DispatchStopReason::ExternalStop;
            break;
        }

        // Check frame budget
        if (blocks_run > 0) {
            auto elapsed_ms = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - slice_start).count();
            if (elapsed_ms >= m_config.frame_budget_ms) {
                result.stop_reason = DispatchStopReason::FrameBudgetExceeded;
                break;
            }
        }

        // Dispatch one block
        bool should_continue = DispatchOneBlock(result);
        blocks_run++;
        cycles_acc += result.cycles;

        if (!should_continue) {
            break;
        }

        // Detect natural exit: RIP moved to 0 or outside image
        const uint64_t rip = m_ctx.rip;
        if (rip == 0 ||
            (rip >= m_base_vaddr && rip < m_base_vaddr + m_image_size) == false) {
            result.stop_reason = DispatchStopReason::Completed;
            break;
        }
    }

    result.guest_rip  = m_ctx.rip;
    result.blocks_run = blocks_run;
    result.cycles     = cycles_acc;

    m_telemetry.RecordJitCycles(cycles_acc);

    return result;
}

DispatchResult Ps5JitDispatchLoop::RunToCompletion() {
    DispatchResult final_result;
    final_result.stop_reason = DispatchStopReason::FrameBudgetExceeded;

    // Override: unlimited budget, run until natural exit
    DispatchConfig saved_cfg = m_config;
    m_config.frame_budget_ms = 1e9;
    m_config.max_blocks_per_slice = UINT32_MAX;

    while (final_result.stop_reason == DispatchStopReason::FrameBudgetExceeded) {
        final_result = RunSlice();
        final_result.blocks_run += final_result.blocks_run;
    }

    m_config = saved_cfg;
    return final_result;
}

// ─── Single block dispatch ────────────────────────────────────────────────────

bool Ps5JitDispatchLoop::DispatchOneBlock(DispatchResult& result) {
    // Translate guest RIP → offset into image buffer
    const uint64_t rip = m_ctx.rip;

    if (rip < m_base_vaddr || rip >= m_base_vaddr + m_image_size) {
        result.stop_reason = DispatchStopReason::Exception;
        result.error_msg   = "RIP 0x" + std::to_string(rip) + " out of image range";
        return false;
    }

    const uint64_t offset = rip - m_base_vaddr;
    const uint8_t* code_ptr = m_image_base + offset;
    const size_t   max_bytes = m_image_size - offset;

    // Record primary opcode for telemetry
    const uint8_t primary_opcode = (max_bytes > 0) ? code_ptr[0] : 0x90;

    const uint64_t t0 = ReadHostCycles();

    // Check code cache first
    auto& block_cache = m_bridge.GetBlockCache();
    if (block_cache.Lookup(rip) != nullptr) {
        // Cache hit — record execution frequency for Tier-2 optimization
        Recompiler::ExecutionTier new_tier = Recompiler::ExecutionTier::Tier0_LazyFastJit;
        if (m_bridge.GetOptimizer().RecordExecution(rip, new_tier)) {
            if (new_tier == Recompiler::ExecutionTier::Tier2_TraceJit) {
                LOGF("[Ps5JitDispatchLoop] Hot block at RIP=0x%llx promoted to Tier-2\n",
                     (unsigned long long)rip);
            }
        }
        m_telemetry.RecordBlockCacheHit(primary_opcode);
        bool ok = m_bridge.ExecuteBlock(m_ctx, code_ptr, max_bytes);
        (void)ok;
        const uint64_t t1 = ReadHostCycles();
        result.cycles += (t1 - t0);
        return true;
    }

    // Cache miss — compile + execute
    auto compiled = m_bridge.CompileAndCacheBlock(code_ptr, max_bytes, rip);
    if (compiled == nullptr) {
        // Unsupported or failed instruction
        m_telemetry.RecordUnsupportedInstruction(
            static_cast<uint32_t>(primary_opcode));
        LOGF("[Ps5JitDispatchLoop] Unsupported instruction at RIP=0x%llx opcode=0x%02x\n",
                 (unsigned long long)rip, (unsigned)primary_opcode);
        if (m_config.stop_on_unsupported) {
            result.stop_reason = DispatchStopReason::UnsupportedInstruction;
            result.error_msg   = "Unsupported instruction: opcode 0x" +
                                 std::to_string(primary_opcode);
            return false;
        }
        // Skip: advance RIP by 1 byte and continue
        m_ctx.rip = rip + 1;
        const uint64_t t1 = ReadHostCycles();
        result.cycles += (t1 - t0);
        return true;
    }

    // Execute the compiled block
    bool ok = m_bridge.ExecuteBlock(m_ctx, code_ptr, max_bytes);
    (void)ok;

    m_telemetry.RecordBlockCompiled(primary_opcode,
                                     /*ir_count=*/4,     // Approximation
                                     /*arm64_count=*/8); // Approximation

    const uint64_t t1 = ReadHostCycles();
    result.cycles += (t1 - t0);
    return true;
}

} // namespace Loader
