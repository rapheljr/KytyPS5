// x86RuntimeBridge.h
//
// Guest CPU Context & Calling Convention Bridge for Phase M Dynamic Recompiler.

#ifndef LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H
#define LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/x86BlockCache.h"

#include <cstdint>

namespace Loader::Recompiler {

struct alignas(16) GuestCpuContext {
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

	uint64_t rip    = 0;
	uint64_t rflags = 0;
};

class X86RuntimeBridge {
public:
	explicit X86RuntimeBridge(size_t cache_size = 16 * 1024 * 1024);
	~X86RuntimeBridge() = default;

	KYTY_CLASS_NO_COPY(X86RuntimeBridge);

	CompiledBlockFunc CompileAndCacheBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip);

	bool ExecuteBlock(GuestCpuContext& ctx, const uint8_t* code_ptr, size_t max_bytes);

	[[nodiscard]] Arm64CodeCache& GetCodeCache() noexcept { return m_code_cache; }
	[[nodiscard]] X86BlockCache& GetBlockCache() noexcept { return m_block_cache; }

private:
	Arm64CodeCache m_code_cache;
	X86BlockCache  m_block_cache;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_X86_RUNTIME_BRIDGE_H
