// ps5JitDispatchLoop.h
//
// PS5 ARM64 JIT Main Dispatch Loop.
//
// Orchestrates the fetch-translate-optimize-execute cycle for one guest thread.
// Integrates:
//   - X86RuntimeBridge  : block compilation, execution
//   - Arm64OptimizationPipeline : post-translation peephole optimizations
//   - JitTelemetryCollector     : all counters
//   - OpcodeCoverageFramework   : opcode distribution histogram
//
// One Ps5JitDispatchLoop per guest OS thread. Thread-safe with respect to the
// shared RadixCodeCache via its internal spinlock.

#ifndef LOADER_PS5_JIT_DISPATCH_LOOP_H
#define LOADER_PS5_JIT_DISPATCH_LOOP_H

#include "common/common.h"
#include "loader/openOrbisElfLoader.h"
#include "loader/recompiler/jitTelemetryCollector.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace Loader {

// ─── Dispatch result ──────────────────────────────────────────────────────────

enum class DispatchStopReason : uint8_t {
    Completed,              // Guest reached entry return / _exit
    FrameBudgetExceeded,    // Time-slice expired — caller should render and resume
    UnsupportedInstruction, // Unimplemented opcode logged, execution halted
    Exception,              // Unrecoverable guest fault
    ExternalStop,           // Caller invoked RequestStop()
};

struct DispatchResult {
    DispatchStopReason stop_reason = DispatchStopReason::Completed;
    uint64_t           guest_rip   = 0;     // PC at stop point
    uint32_t           blocks_run  = 0;     // Blocks executed this slice
    uint64_t           cycles      = 0;     // Host cycles consumed
    std::string        error_msg;
};

// ─── Dispatch loop configuration ─────────────────────────────────────────────

struct DispatchConfig {
    uint32_t max_blocks_per_slice   = 4096;  // Yield back to renderer after N blocks
    double   frame_budget_ms        = 16.67; // 60 FPS default
    bool     enable_optimization    = true;  // Run Arm64OptimizationPipeline
    bool     stop_on_unsupported    = false; // If false: log & skip; if true: halt
    uint64_t max_guest_instructions = 0;     // 0 = unlimited
};

// ─── Main dispatch loop ───────────────────────────────────────────────────────

class Ps5JitDispatchLoop {
public:
    explicit Ps5JitDispatchLoop(Recompiler::X86RuntimeBridge& bridge,
                                Recompiler::JitTelemetryCollector& telemetry);
    ~Ps5JitDispatchLoop() = default;

    KYTY_CLASS_NO_COPY(Ps5JitDispatchLoop);

    // Setup the initial guest CPU context from a loaded ELF.
    void SetupFromLoadResult(const OpenOrbisLoadResult& result);

    // Configure dispatch parameters.
    void Configure(const DispatchConfig& cfg) { m_config = cfg; }

    // Run one frame slice. Returns when budget exhausted or execution ends.
    [[nodiscard]] DispatchResult RunSlice();

    // Run to completion (blocks until guest exits or error).
    [[nodiscard]] DispatchResult RunToCompletion();

    // Signal the loop to stop at the next slice boundary (thread-safe).
    void RequestStop() noexcept { m_stop_requested.store(true, std::memory_order_release); }

    // Reset stop flag and guest PC for re-entry.
    void Reset(uint64_t entry_vaddr);

    [[nodiscard]] const Recompiler::GuestCpuContext& GetCpuContext() const noexcept {
        return m_ctx;
    }
    [[nodiscard]] bool IsRunning() const noexcept {
        return m_stop_requested.load(std::memory_order_acquire) == false;
    }

private:
    // Translate + execute one guest basic block at m_ctx.rip.
    // Returns false if execution should halt.
    bool DispatchOneBlock(DispatchResult& result);

    Recompiler::X86RuntimeBridge&       m_bridge;
    Recompiler::JitTelemetryCollector&  m_telemetry;
    Recompiler::GuestCpuContext         m_ctx;
    DispatchConfig                      m_config;
    std::atomic<bool>                   m_stop_requested{false};

    // Guest image backing pointer (for block lookup)
    const uint8_t* m_image_base = nullptr;
    uint64_t       m_image_size = 0;
    uint64_t       m_base_vaddr = 0;
};

} // namespace Loader

#endif // LOADER_PS5_JIT_DISPATCH_LOOP_H
