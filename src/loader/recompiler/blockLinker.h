// blockLinker.h
//
// Direct Block Linking & Branch Target Patching Engine for KytyPS5 ARM64 Recompiler.

#ifndef LOADER_RECOMPILER_BLOCK_LINKER_H
#define LOADER_RECOMPILER_BLOCK_LINKER_H

#include "common/common.h"
#include "loader/recompiler/x86BlockCache.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

enum class LinkType : uint8_t {
	DirectJump,
	DirectCall,
	FarJump,
	LazyResolverStub
};

struct BlockLinkSite {
	uint8_t* patch_site_addr  = nullptr;
	uint64_t source_guest_rip = 0;
	uint64_t target_guest_rip = 0;
	LinkType link_type        = LinkType::DirectJump;
};

class BlockLinker {
public:
	BlockLinker() = default;
	~BlockLinker() = default;

	KYTY_CLASS_NO_COPY(BlockLinker);

	// 1. Stub Emission
	static size_t EmitLazyLinkStub(uint8_t* code_ptr, uint64_t target_guest_rip);
	static size_t EmitFarJumpStub(uint8_t* code_ptr, const void* target_host_addr);

	// 2. In-Place Branch Target Patching
	static bool PatchBranchTarget(uint8_t* patch_site, const void* target_host_addr, LinkType link_type);

	// 3. Link Management & Resolution
	void RegisterLinkSite(uint8_t* patch_site, uint64_t source_rip, uint64_t target_rip, LinkType link_type);
	size_t ResolvePendingLinks(uint64_t target_guest_rip, const void* target_host_addr);
	size_t InvalidateLinksForBlock(uint64_t guest_rip);

	void Clear() noexcept;

private:
	std::unordered_map<uint64_t, std::vector<BlockLinkSite>> m_pending_links; // target_rip -> link sites
	std::unordered_map<uint64_t, std::vector<BlockLinkSite>> m_resolved_links; // source_rip -> link sites
	std::mutex                                               m_linker_mutex;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_BLOCK_LINKER_H
