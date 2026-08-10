// jitBlockLinker.cpp
//
// Direct Host Basic Block Linking & Dynamic Edge Invalidation Implementation.

#include "loader/recompiler/jitBlockLinker.h"
#include "loader/recompiler/arm64Emitter.h"
#include "loader/recompiler/x86BlockCache.h"

#include <cstring>

namespace Loader::Recompiler {

uint32_t JitBlockLinker::EncodeBranch(int64_t offset_bytes) {
	// B imm26: bits [31:26] = 000101, imm26 = offset >> 2
	int64_t imm26 = (offset_bytes >> 2) & 0x03FFFFFF;
	return 0x14000000u | static_cast<uint32_t>(imm26);
}

uint32_t JitBlockLinker::EncodeConditionalBranch(int64_t offset_bytes, BranchCondition cond) {
	// B.cond imm19: bits [31:24] = 01010100, imm19 = offset >> 2, cond = bits [3:0]
	int64_t imm19 = (offset_bytes >> 2) & 0x0007FFFF;
	return 0x54000000u | (static_cast<uint32_t>(imm19) << 5u) | (static_cast<uint8_t>(cond) & 0x0Fu);
}

bool JitBlockLinker::PatchBranch(uint8_t* branch_site, uint8_t* target_addr, bool is_conditional, BranchCondition cond) {
	if (!branch_site || !target_addr) return false;

	int64_t offset = reinterpret_cast<intptr_t>(target_addr) - reinterpret_cast<intptr_t>(branch_site);
	uint32_t branch_inst = is_conditional ? EncodeConditionalBranch(offset, cond) : EncodeBranch(offset);

	Arm64CodeCache::SetJitWriteProtect(false);
	std::memcpy(branch_site, &branch_inst, sizeof(branch_inst));
	Arm64CodeCache::SetJitWriteProtect(true);
	Arm64CodeCache::FlushInstructionCache(branch_site, sizeof(branch_inst));

	return true;
}

void JitBlockLinker::RegisterBlock(uint64_t guest_rip, uint8_t* host_code, const std::vector<BlockLinkEdge>& edges) {
	std::lock_guard<std::mutex> lock(m_mutex);

	BlockLinkInfo info{};
	info.guest_rip       = guest_rip;
	info.host_block_addr = host_code;
	info.outgoing_edges  = edges;

	m_blocks[guest_rip] = info;

	// Resolve any already-compiled outgoing links from this block
	for (auto& edge : info.outgoing_edges) {
		auto it = m_blocks.find(edge.target_guest_rip);
		if (it != m_blocks.end() && it->second.host_block_addr != nullptr) {
			if (PatchBranch(edge.branch_site_host_addr, it->second.host_block_addr, edge.is_conditional, edge.condition)) {
				it->second.incoming_branch_sites.push_back(edge.branch_site_host_addr);
				m_active_links++;
			}
		}
	}

	// Resolve any existing incoming branches waiting for this block
	for (auto& pair : m_blocks) {
		if (pair.first == guest_rip) continue;
		for (auto& edge : pair.second.outgoing_edges) {
			if (edge.target_guest_rip == guest_rip) {
				if (PatchBranch(edge.branch_site_host_addr, host_code, edge.is_conditional, edge.condition)) {
					m_blocks[guest_rip].incoming_branch_sites.push_back(edge.branch_site_host_addr);
					m_active_links++;
				}
			}
		}
	}
}

void JitBlockLinker::TryLinkBlock(uint64_t guest_rip) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_blocks.find(guest_rip);
	if (it == m_blocks.end()) return;

	for (auto& edge : it->second.outgoing_edges) {
		auto target_it = m_blocks.find(edge.target_guest_rip);
		if (target_it != m_blocks.end() && target_it->second.host_block_addr != nullptr) {
			if (PatchBranch(edge.branch_site_host_addr, target_it->second.host_block_addr, edge.is_conditional, edge.condition)) {
				target_it->second.incoming_branch_sites.push_back(edge.branch_site_host_addr);
				m_active_links++;
			}
		}
	}
}

void JitBlockLinker::ResolveIncomingLinks(uint64_t target_guest_rip, uint8_t* target_host_addr) {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& pair : m_blocks) {
		for (auto& edge : pair.second.outgoing_edges) {
			if (edge.target_guest_rip == target_guest_rip) {
				if (PatchBranch(edge.branch_site_host_addr, target_host_addr, edge.is_conditional, edge.condition)) {
					m_blocks[target_guest_rip].incoming_branch_sites.push_back(edge.branch_site_host_addr);
					m_active_links++;
				}
			}
		}
	}
}

void JitBlockLinker::UnlinkBlock(uint64_t guest_rip) {
	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_blocks.find(guest_rip);
	if (it == m_blocks.end()) return;

	// For all incoming branches targeting this block, revert them to their fallback exit stubs
	for (auto* branch_site : it->second.incoming_branch_sites) {
		for (auto& pair : m_blocks) {
			for (auto& edge : pair.second.outgoing_edges) {
				if (edge.branch_site_host_addr == branch_site && edge.fallback_exit_stub != nullptr) {
					PatchBranch(edge.branch_site_host_addr, edge.fallback_exit_stub, false);
					if (m_active_links > 0) m_active_links--;
				}
			}
		}
	}

	it->second.incoming_branch_sites.clear();
}

size_t JitBlockLinker::GetRegisteredBlockCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_blocks.size();
}

size_t JitBlockLinker::GetActiveLinkCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_active_links;
}

void JitBlockLinker::Clear() noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	m_blocks.clear();
	m_active_links = 0;
}

} // namespace Loader::Recompiler
