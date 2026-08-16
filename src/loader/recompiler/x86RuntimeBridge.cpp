// x86RuntimeBridge.cpp
//
// Guest CPU Context & Calling Convention Bridge for Phase M Dynamic Recompiler.

#include "loader/recompiler/x86RuntimeBridge.h"
#include "loader/recompiler/arm64IRCodegen.h"
#include "loader/recompiler/irOptimizationPasses.h"
#include "loader/recompiler/x86ToIRLowering.h"

#include <stdexcept>

namespace Loader::Recompiler {

X86RuntimeBridge::X86RuntimeBridge(size_t cache_size)
    : m_code_cache(cache_size), m_block_cache(65536) {}

CompiledBlockFunc X86RuntimeBridge::CompileAndCacheBlock(const uint8_t* code_ptr, size_t max_bytes, uint64_t guest_rip) {
	if (!code_ptr || max_bytes == 0) return nullptr;

	// 1. Check if block already exists in block cache
	CompiledBlockFunc cached_func = m_block_cache.Lookup(guest_rip);
	if (cached_func) return cached_func;

	// 2. Decode & Lower to SSA Compiler IR
	X86ToIRLowering lowering;
	auto cfg = lowering.LowerBlock(code_ptr, max_bytes, guest_rip);
	if (!cfg) return nullptr;

	// 3. Execute 9 Optimization Passes
	PassManager pm = PassManager::CreateDefaultPipeline();
	pm.RunAll(*cfg);

	// 4. Emit ARM64 Machine Code using Linear Scan Register Allocator
	Arm64Emitter emitter;
	Arm64IRCodegen codegen;
	if (!codegen.CompileCFG(*cfg, emitter)) {
		return nullptr;
	}

	// 5. Allocate in Executable Code Cache
	size_t code_bytes = emitter.GetCodeSizeBytes();
	uint8_t* host_code_ptr = m_code_cache.AllocateCode(code_bytes);
	if (!host_code_ptr) return nullptr;

	Arm64CodeCache::SetJitWriteProtect(false);
	std::memcpy(host_code_ptr, emitter.GetCode().data(), code_bytes);
	Arm64CodeCache::FlushInstructionCache(host_code_ptr, code_bytes);
	Arm64CodeCache::SetJitWriteProtect(true);

	CompiledBlockFunc func = reinterpret_cast<CompiledBlockFunc>(host_code_ptr);
	m_block_cache.Insert(guest_rip, func);

	// 6. Register Relocations & Patch Known Direct Links
	for (const auto& reloc : emitter.GetRelocations()) {
		LinkType lt = reloc.is_branch_link ? LinkType::DirectCall : LinkType::DirectJump;
		m_linker.RegisterLinkSite(
			host_code_ptr + reloc.host_code_offset,
			guest_rip,
			reloc.target_guest_rip,
			lt
		);

		CompiledBlockFunc target_func = m_block_cache.Lookup(reloc.target_guest_rip);
		if (target_func) {
			BlockLinker::PatchBranchTarget(
				host_code_ptr + reloc.host_code_offset,
				reinterpret_cast<const void*>(target_func),
				lt
			);
		}
	}

	// 7. Resolve Pending Links in Direct Block Linker
	m_linker.ResolvePendingLinks(guest_rip, host_code_ptr);

	return func;
}

bool X86RuntimeBridge::ExecuteBlock(GuestCpuContext& ctx, const uint8_t* code_ptr, size_t max_bytes) {
	if (!code_ptr || max_bytes == 0) return false;

	// 1. Stack Alignment Verification ((RSP & 0xF) == 0)
	if (!ctx.VerifyStackAlignment()) {
		// Auto-fix unaligned stack frame
		ctx.rsp = (ctx.rsp & ~0x0Fu);
	}

	// 2. Lookup or Compile Block
	CompiledBlockFunc func = CompileAndCacheBlock(code_ptr, max_bytes, ctx.rip);
	if (!func) return false;

	// 3. Execute JIT Machine Code & Register Flush
	func(&ctx);
	ctx.FlushLazyRegisters();
	return true;
}

} // namespace Loader::Recompiler
