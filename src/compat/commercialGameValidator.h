// commercialGameValidator.h
//
// Commercial PS5 Game Subsystem Validation Engine.
//
// Automated validation for 10 core commercial game subsystems:
//   1. Large Executable Loading (>100MB ELF binaries, heavy symbol tables)
//   2. Millions of Translated Instructions (radix code cache stress, multi-million dispatches)
//   3. Thread Synchronization (Mutex, Condvar, Futex / ps5Umtx, Atomics)
//   4. Exception Handling & Signal Recovery (SIGSEGV page fault handling, stack unwinding)
//   5. Dynamic Libraries (PRX relocation tables, dynamic symbol resolution, cross-PRX calls)
//   6. GPU Command Streams (PM4 packet parsing, ring buffer sync, draw/compute submission)
//   7. Shader Compilation (SPIR-V translation, pipeline state objects, push constants)
//   8. Memory Management (sceKernelMmap, R/W/X page permissions, alignment)
//   9. Save States (CPU register snapshotting, thread state, VFS mount state)
//  10. Full Subsystem Integration Health Test

#ifndef COMPAT_COMMERCIAL_GAME_VALIDATOR_H
#define COMPAT_COMMERCIAL_GAME_VALIDATOR_H

#include "common/common.h"
#include "compat/titleCompatibility.h"
#include "loader/openOrbisElfLoader.h"
#include "loader/ps5JitDispatchLoop.h"
#include "loader/recompiler/jitTelemetryCollector.h"
#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Compat {

enum class SubsystemValidationType {
    LargeExecutable = 0,
    MillionInstructions,
    ThreadSynchronization,
    ExceptionHandling,
    DynamicLibraries,
    GpuCommandStreams,
    ShaderCompilation,
    MemoryManagement,
    SaveStates,
    IntegrationHealth
};

const char* SubsystemValidationTypeToString(SubsystemValidationType type);

struct SubsystemTestResult {
    SubsystemValidationType type;
    std::string name;
    bool        passed = false;
    uint64_t    duration_ms = 0;
    std::string details;
    std::string error_trace;
};

struct CommercialValidationReport {
    std::string title_id;
    std::string title_name;
    bool        overall_success = false;
    uint32_t    tests_passed    = 0;
    uint32_t    tests_failed    = 0;
    uint64_t    total_duration_ms = 0;

    std::vector<SubsystemTestResult> results;
    std::vector<std::string>          unsupported_syscalls;
    std::vector<std::string>          unsupported_opcodes;
};

class CommercialGameValidator {
public:
    CommercialGameValidator() = default;
    ~CommercialGameValidator() = default;

    KYTY_CLASS_NO_COPY(CommercialGameValidator);

    /// Run all 10 automated commercial subsystem validation tests for @p title_id.
    [[nodiscard]] CommercialValidationReport ValidateGame(const std::string& title_id,
                                                         const std::string& title_name = "Commercial Game");

    // Individual subsystem validation methods
    [[nodiscard]] SubsystemTestResult ValidateLargeExecutable();
    [[nodiscard]] SubsystemTestResult ValidateMillionInstructions();
    [[nodiscard]] SubsystemTestResult ValidateThreadSynchronization();
    [[nodiscard]] SubsystemTestResult ValidateExceptionHandling();
    [[nodiscard]] SubsystemTestResult ValidateDynamicLibraries();
    [[nodiscard]] SubsystemTestResult ValidateGpuCommandStreams();
    [[nodiscard]] SubsystemTestResult ValidateShaderCompilation();
    [[nodiscard]] SubsystemTestResult ValidateMemoryManagement();
    [[nodiscard]] SubsystemTestResult ValidateSaveStates();
    [[nodiscard]] SubsystemTestResult ValidateIntegrationHealth();
};

} // namespace Compat

#endif // COMPAT_COMMERCIAL_GAME_VALIDATOR_H
