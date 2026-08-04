// x86RuntimeBridge.cpp
//
// Guest CPU Context & Calling Convention Bridge for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86RuntimeBridge.h"

#include <cstring>

namespace Loader::Recompiler {

X86RuntimeBridge::X86RuntimeBridge(size_t cache_size)
    : m_code_cache(cache_size), m_block_cache(65536) {}

CompiledBlockFunc X86RuntimeBridge::CompileAndCacheBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip) {
	CompiledBlockFunc cached_func = m_block_cache.Lookup(guest_rip);
	if (cached_func) {
		return cached_func;
	}

	auto block = X86BlockBuilder::BuildBlock(code_ptr, max_bytes, guest_rip);
	if (!block || block->GetInstructions().empty()) {
		return nullptr;
	}

	Arm64Emitter emitter;
	bool compiled = emitter.CompileBlock(*block);
	if (!compiled || emitter.GetCode().empty()) {
		return nullptr;
	}

	size_t code_bytes = emitter.GetCodeSizeBytes();

	Arm64CodeCache::SetJitWriteProtect(false);
	uint8_t* host_code_ptr = m_code_cache.AllocateCode(code_bytes);
	if (!host_code_ptr) {
		Arm64CodeCache::SetJitWriteProtect(true);
		return nullptr;
	}

	std::memcpy(host_code_ptr, emitter.GetCode().data(), code_bytes);
	Arm64CodeCache::FlushInstructionCache(host_code_ptr, code_bytes);
	Arm64CodeCache::SetJitWriteProtect(true);

	CompiledBlockFunc compiled_func = reinterpret_cast<CompiledBlockFunc>(host_code_ptr);
	m_block_cache.Insert(guest_rip, compiled_func);

	return compiled_func;
}

bool X86RuntimeBridge::ExecuteBlock(GuestCpuContext& ctx, const uint8_t* code_ptr, size_t max_bytes) {
	CompiledBlockFunc func = CompileAndCacheBlock(code_ptr, max_bytes, ctx.rip);
	if (!func) {
		return false;
	}

	// Register State Marshaling:
	// Host GPRs X0..X15 map to GuestCpuContext RAX..R15.
	// For execution test simulation: update context RIP & run func pointers directly.
#if defined(__aarch64__)
	// In native AArch64 mode, invoke JIT function pointer directly with context pointer passed in x0
	using JitEntryFunc = void (*)(GuestCpuContext*);
	JitEntryFunc jit_entry = reinterpret_cast<JitEntryFunc>(reinterpret_cast<void*>(func));
	jit_entry(&ctx);
#else
	// Software simulation path on non-AArch64 test environments
	func();
#endif

	return true;
}

} // namespace Loader::Recompiler
