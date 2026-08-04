// blockLinker.cpp
//
// Direct Block Linking & Branch Target Patching Engine for KytyPS5 ARM64 Recompiler.

#include "loader/recompiler/blockLinker.h"
#include "loader/recompiler/arm64EncoderHelpers.h"

#include <cmath>
#include <cstring>

namespace Loader::Recompiler {

using namespace Arm64EncoderHelper;

size_t BlockLinker::EmitLazyLinkStub(uint8_t* code_ptr, uint64_t target_guest_rip) {
	// Emits:
	//   MOVZ X16, #lower16(rip)
	//   MOVK X16, #hw1(rip), LSL #16
	//   MOVK X16, #hw2(rip), LSL #32
	//   MOVK X16, #hw3(rip), LSL #48
	//   RET
	uint32_t* insts = reinterpret_cast<uint32_t*>(code_ptr);
	uint16_t h0 = target_guest_rip & 0xFFFFu;
	uint16_t h1 = (target_guest_rip >> 16u) & 0xFFFFu;
	uint16_t h2 = (target_guest_rip >> 32u) & 0xFFFFu;
	uint16_t h3 = (target_guest_rip >> 48u) & 0xFFFFu;

	insts[0] = MoveWide(true, 2, 0, h0, 16); // MOVZ X16, #h0
	insts[1] = MoveWide(true, 3, 1, h1, 16); // MOVK X16, #h1
	insts[2] = MoveWide(true, 3, 2, h2, 16); // MOVK X16, #h2
	insts[3] = MoveWide(true, 3, 3, h3, 16); // MOVK X16, #h3
	insts[4] = 0xD65F03C0u;                   // RET

	Arm64CodeCache::FlushInstructionCache(code_ptr, 5 * sizeof(uint32_t));
	return 5 * sizeof(uint32_t);
}

size_t BlockLinker::EmitFarJumpStub(uint8_t* code_ptr, const void* target_host_addr) {
	// Emits:
	//   LDR X16, [PC, #8] -> 0x58000050
	//   BR X16           -> 0xD61F0200
	//   .quad target_host_addr
	uint32_t* insts = reinterpret_cast<uint32_t*>(code_ptr);
	insts[0] = 0x58000050u; // LDR X16, [PC, #8]
	insts[1] = 0xD61F0200u; // BR X16

	uint64_t* target_slot = reinterpret_cast<uint64_t*>(code_ptr + 8);
	*target_slot = reinterpret_cast<uint64_t>(target_host_addr);

	Arm64CodeCache::FlushInstructionCache(code_ptr, 16);
	return 16;
}

bool BlockLinker::PatchBranchTarget(uint8_t* patch_site, const void* target_host_addr, LinkType link_type) {
	if (!patch_site || !target_host_addr) return false;

	intptr_t offset = reinterpret_cast<intptr_t>(target_host_addr) - reinterpret_cast<intptr_t>(patch_site);
	intptr_t offset_words = offset / 4;

	Arm64CodeCache::SetJitWriteProtect(false);

	if (std::labs(offset_words) < (1L << 25)) {
		// Fits within AArch64 26-bit relative branch (±128MB)
		uint32_t patch_inst = BranchUncond(link_type == LinkType::DirectCall, static_cast<int32_t>(offset_words));
		*reinterpret_cast<uint32_t*>(patch_site) = patch_inst;
		Arm64CodeCache::FlushInstructionCache(patch_site, sizeof(uint32_t));
	} else {
		// Exceeds ±128MB -> Use Far Jump Stub
		EmitFarJumpStub(patch_site, target_host_addr);
	}

	Arm64CodeCache::SetJitWriteProtect(true);
	return true;
}

void BlockLinker::RegisterLinkSite(uint8_t* patch_site, uint64_t source_rip, uint64_t target_rip, LinkType link_type) {
	std::lock_guard<std::mutex> lock(m_linker_mutex);
	BlockLinkSite site{patch_site, source_rip, target_rip, link_type};
	m_pending_links[target_rip].push_back(site);
}

size_t BlockLinker::ResolvePendingLinks(uint64_t target_guest_rip, const void* target_host_addr) {
	std::lock_guard<std::mutex> lock(m_linker_mutex);
	auto it = m_pending_links.find(target_guest_rip);
	if (it == m_pending_links.end()) return 0;

	size_t patched_count = 0;
	for (const auto& site : it->second) {
		if (PatchBranchTarget(site.patch_site_addr, target_host_addr, site.link_type)) {
			m_resolved_links[site.source_guest_rip].push_back(site);
			patched_count++;
		}
	}

	m_pending_links.erase(it);
	return patched_count;
}

size_t BlockLinker::InvalidateLinksForBlock(uint64_t guest_rip) {
	std::lock_guard<std::mutex> lock(m_linker_mutex);
	size_t unlinked_count = 0;

	// 1. Unlink incoming links targeting guest_rip
	for (auto& [src_rip, sites] : m_resolved_links) {
		for (auto& site : sites) {
			if (site.target_guest_rip == guest_rip) {
				// Re-emit Lazy Resolver Stub at patch site
				Arm64CodeCache::SetJitWriteProtect(false);
				EmitLazyLinkStub(site.patch_site_addr, guest_rip);
				Arm64CodeCache::SetJitWriteProtect(true);

				m_pending_links[guest_rip].push_back(site);
				unlinked_count++;
			}
		}
	}

	// 2. Erase outgoing links from guest_rip
	m_resolved_links.erase(guest_rip);
	return unlinked_count;
}

void BlockLinker::Clear() noexcept {
	std::lock_guard<std::mutex> lock(m_linker_mutex);
	m_pending_links.clear();
	m_resolved_links.clear();
}

} // namespace Loader::Recompiler
