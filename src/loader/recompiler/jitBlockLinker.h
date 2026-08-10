// jitBlockLinker.h
//
// Direct Host Basic Block Linking & Dynamic Edge Invalidation Engine.
// Allows ARM64 translated basic blocks to branch directly to target blocks,
// bypassing the x86RuntimeBridge dispatcher loop for hot loops and branches.

#ifndef LOADER_RECOMPILER_JIT_BLOCK_LINKER_H
#define LOADER_RECOMPILER_JIT_BLOCK_LINKER_H

#include "common/common.h"
#include "loader/recompiler/arm64Emitter.h"

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Loader::Recompiler {

enum class BranchCondition : uint8_t {
	EQ = 0x0, NE = 0x1, CS = 0x2, CC = 0x3,
	MI = 0x4, PL = 0x5, VS = 0x6, VC = 0x7,
	HI = 0x8, LS = 0x9, GE = 0xA, LT = 0xB,
	GT = 0xC, LE = 0xD, AL = 0xE, NV = 0xF
};

struct BlockLinkEdge {
	uint8_t* branch_site_host_addr = nullptr; // Location of the 32-bit branch instruction
	uint64_t target_guest_rip       = 0;       // Destination guest RIP
	bool     is_conditional         = false;   // True if B.cond, false if B
	BranchCondition condition       = BranchCondition::AL;
	uint8_t* fallback_exit_stub     = nullptr; // Dispatcher return stub if unlinked
};

struct BlockLinkInfo {
	uint64_t                   guest_rip       = 0;
	uint8_t*                   host_block_addr = nullptr;
	std::vector<BlockLinkEdge> outgoing_edges;
	std::vector<uint8_t*>      incoming_branch_sites;
};

class JitBlockLinker {
public:
	JitBlockLinker() = default;
	~JitBlockLinker() = default;

	KYTY_CLASS_NO_COPY(JitBlockLinker);

	/// Register a freshly compiled basic block and its exit edges
	void RegisterBlock(uint64_t guest_rip, uint8_t* host_code, const std::vector<BlockLinkEdge>& edges);

	/// Attempt to patch all outgoing edges of a block to already-compiled destination blocks
	void TryLinkBlock(uint64_t guest_rip);

	/// Attempt to resolve any incoming edges from existing blocks that target this newly compiled block
	void ResolveIncomingLinks(uint64_t target_guest_rip, uint8_t* target_host_addr);

	/// Unlink all incoming branches to this block (reverting them to their fallback dispatcher exit stubs)
	void UnlinkBlock(uint64_t guest_rip);

	/// Directly patch an ARM64 branch instruction (B or B.cond) in executable memory
	static bool PatchBranch(uint8_t* branch_site, uint8_t* target_addr, bool is_conditional, BranchCondition cond = BranchCondition::AL);

	/// Encode an unconditional ARM64 branch (B offset)
	static uint32_t EncodeBranch(int64_t offset_bytes);

	/// Encode a conditional ARM64 branch (B.cond offset)
	static uint32_t EncodeConditionalBranch(int64_t offset_bytes, BranchCondition cond);

	[[nodiscard]] size_t GetRegisteredBlockCount() const noexcept;
	[[nodiscard]] size_t GetActiveLinkCount() const noexcept;

	void Clear() noexcept;

private:
	mutable std::mutex                           m_mutex;
	std::unordered_map<uint64_t, BlockLinkInfo>  m_blocks;
	size_t                                       m_active_links = 0;
};

} // namespace Loader::Recompiler

#endif // LOADER_RECOMPILER_JIT_BLOCK_LINKER_H
