// x86RuntimeBridge.h
//
// Guest CPU Context & Calling Convention Bridge for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H
#define LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/blockLinker.h"
#include "loader/recompiler/runtimeOptimizationEngine.h"
#include "loader/recompiler/x86BlockCache.h"

#include <cstdint>
#include <exception>

namespace Loader::Recompiler {

struct alignas(16) GuestCpuContext {
	// 1. General Purpose Registers (GPRs)
	uint64_t rax = 0; // X0
	uint64_t rcx = 0; // X1
	uint64_t rdx = 0; // X2
	uint64_t rbx = 0; // X3
	uint64_t rsp = 0; // X4
	uint64_t rbp = 0; // X5
	uint64_t rsi = 0; // X6
	uint64_t rdi = 0; // X7
	uint64_t r8  = 0; // X8
	uint64_t r9  = 0; // X9
	uint64_t r10 = 0; // X10
	uint64_t r11 = 0; // X11
	uint64_t r12 = 0; // X12
	uint64_t r13 = 0; // X13
	uint64_t r14 = 0; // X14
	uint64_t r15 = 0; // X15

	// 2. Vector SIMD Registers (128-Bit XMM & 256-Bit YMM High)
	alignas(16) uint64_t xmm[16][2]{};    // XMM0..XMM15 (128-bit)
	alignas(16) uint64_t ymm_hi[16][2]{}; // YMM0_hi..YMM15_hi (high 128-bit for 256-bit AVX)

	// 3. Status & Control State
	bool     avx_state_active = false;
	uint32_t mxcsr            = 0x1F80; // Default IEEE-754 control/status mask
	uint64_t rflags           = 0x02;   // x86 CPU Flags
	uint64_t rip              = 0;      // Instruction Pointer

	// 4. Lazy Register Synchronization Masks
	uint32_t dirty_gpr_mask = 0;
	uint32_t dirty_xmm_mask = 0;

	void SetGprDirty(uint8_t reg_idx) noexcept { dirty_gpr_mask |= (1u << reg_idx); }
	void SetXmmDirty(uint8_t reg_idx) noexcept { dirty_xmm_mask |= (1u << reg_idx); }
	void FlushLazyRegisters() noexcept { dirty_gpr_mask = 0; dirty_xmm_mask = 0; }

	[[nodiscard]] bool VerifyStackAlignment() const noexcept {
		return (rsp & 0x0Fu) == 0;
	}
};

class X86RuntimeBridge {
public:
	explicit X86RuntimeBridge(size_t cache_size = 16 * 1024 * 1024);
	~X86RuntimeBridge() = default;

	KYTY_CLASS_NO_COPY(X86RuntimeBridge);

	CompiledBlockFunc CompileAndCacheBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip);

	// Fast ABI Transition Trampoline & Exception-Safe Frame Execution
	bool ExecuteBlock(GuestCpuContext& ctx, const uint8_t* code_ptr, size_t max_bytes);

	[[nodiscard]] Arm64CodeCache& GetCodeCache() noexcept { return m_code_cache; }
	[[nodiscard]] X86BlockCache& GetBlockCache() noexcept { return m_block_cache; }
	[[nodiscard]] RuntimeOptimizationEngine& GetOptimizer() noexcept { return m_optimizer; }
	[[nodiscard]] BlockLinker& GetLinker() noexcept { return m_linker; }

private:
	Arm64CodeCache            m_code_cache;
	X86BlockCache             m_block_cache;
	RuntimeOptimizationEngine m_optimizer;
	BlockLinker               m_linker;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H
