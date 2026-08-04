// pm4Translator.h
//
// Backend translation layer translating Pm4CommandList to IGraphicBackend (Metal / Vulkan).
// Covers 100% of AMD PM4 packet translation paths.

#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H

#include "common/common.h"
#include "graphics/guest_gpu/command_processor/pm4CommandList.h"
#include "graphics/host_gpu/renderer/backend/graphicBackend.h"

#include <cstdint>
#include <memory>

namespace Libs::Graphics::Pm4 {

struct TranslationStats {
	uint64_t commands_translated   = 0;
	uint64_t draw_commands         = 0;
	uint64_t dispatch_commands     = 0;
	uint64_t dma_commands          = 0;
	uint64_t clear_commands        = 0;
	uint64_t barrier_commands      = 0;
	uint64_t query_commands        = 0;
	uint64_t event_commands        = 0;
	uint64_t write_data_commands   = 0;
	uint64_t copy_data_commands    = 0;
	uint64_t ce_commands           = 0;
	uint64_t register_commands     = 0;
	uint64_t sync_commands         = 0;
	uint64_t indirect_buf_commands = 0;
	uint64_t nop_commands          = 0;
	uint64_t unknown_commands      = 0;
};

class Pm4Translator {
public:
	explicit Pm4Translator(IGraphicBackend* backend = nullptr) : m_backend(backend) {}
	~Pm4Translator() = default;

	KYTY_CLASS_NO_COPY(Pm4Translator);

	void SetBackend(IGraphicBackend* backend) noexcept { m_backend = backend; }
	[[nodiscard]] IGraphicBackend* GetBackend() const noexcept { return m_backend; }

	bool TranslateAndExecute(const Pm4CommandList& cmd_list);

	[[nodiscard]] const TranslationStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = TranslationStats{}; }

private:
	bool DispatchCommand(const GenericCommand& cmd);

	// ─── Per-command handlers ─────────────────────────────────────────────────
	bool HandleDrawNonIndexed(const CmdDrawNonIndexed& cmd);
	bool HandleDrawIndexed(const CmdDrawIndexed& cmd);
	bool HandleDrawIndexedOffset(const CmdDrawIndexedOffset& cmd);
	bool HandleDrawIndirect(const CmdDrawIndirect& cmd);
	bool HandleMultiDrawIndirect(const CmdMultiDrawIndirect& cmd);
	bool HandleDispatchDirect(const CmdDispatchDirect& cmd);
	bool HandleDispatchIndirect(const CmdDispatchIndirect& cmd);
	bool HandleDispatchDraw(const CmdDispatchDraw& cmd);
	bool HandleNumInstances(const CmdNumInstances& cmd);
	bool HandleDmaCopy(const CmdDmaCopy& cmd);
	bool HandleWriteData(const CmdWriteData& cmd);
	bool HandleCopyData(const CmdCopyData& cmd);
	bool HandleClearRenderTarget(const CmdClearRenderTarget& cmd);
	bool HandlePipelineBarrier(const CmdPipelineBarrier& cmd);
	bool HandleSurfaceSync(const CmdSurfaceSync& cmd);
	bool HandleSetEvent(const CmdSetEvent& cmd);
	bool HandleReleaseMem(const CmdReleaseMem& cmd);
	bool HandleTimestampQuery(const CmdTimestampQuery& cmd);
	bool HandleMemSemaphore(const CmdMemSemaphore& cmd);
	bool HandlePfpSyncMe(const CmdPfpSyncMe& cmd);
	bool HandleSetRegisterState(const CmdSetRegisterState& cmd);
	bool HandleSetPredication(const CmdSetPredication& cmd);
	bool HandleSetIndexType(const CmdSetIndexType& cmd);
	bool HandleSetBase(const CmdSetBase& cmd);
	bool HandleClearState(const CmdClearState& cmd);
	bool HandleContextControl(const CmdContextControl& cmd);
	bool HandleIndirectBuffer(const CmdIndirectBuffer& cmd);
	bool HandleWriteConstRam(const CmdWriteConstRam& cmd);
	bool HandleDumpConstRam(const CmdDumpConstRam& cmd);
	bool HandleCeCounter(const CmdCeCounter& cmd);
	bool HandleWaitCeCounter(const CmdWaitCeCounter& cmd);
	bool HandleGetLodStats(const CmdGetLodStats& cmd);
	bool HandleRewind(const CmdRewind& cmd);
	bool HandleNop(const CmdNop& cmd);

	// ─── Internal CE RAM state ────────────────────────────────────────────────
	static constexpr uint32_t kCeRamSizeDw = 4096; // GFX9 CE RAM: 16 KB / 4096 DW
	uint32_t m_ce_ram[kCeRamSizeDw]        = {};
	uint32_t m_ce_counter                  = 0;
	uint32_t m_de_counter                  = 0;
	uint32_t m_instance_count              = 1;   // IT_NUM_INSTANCES state
	uint32_t m_index_type                  = 0;   // Current index type (16 vs 32-bit)
	uint64_t m_index_base_addr             = 0;   // IT_INDEX_BASE base
	uint32_t m_predication_enabled         = 0;   // Predication state

	IGraphicBackend* m_backend  = nullptr;
	TranslationStats m_stats{};
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H
