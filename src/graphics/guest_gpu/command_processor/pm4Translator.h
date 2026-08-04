// pm4Translator.h
//
// Backend translation layer translating Pm4CommandList to IGraphicBackend (Metal / Vulkan) for Phase K.

#ifndef GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H
#define GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H

#include "common/common.h"
#include "graphics/guest_gpu/command_processor/pm4CommandList.h"
#include "graphics/host_gpu/renderer/backend/graphicBackend.h"

#include <cstdint>
#include <memory>

namespace Libs::Graphics::Pm4 {

struct TranslationStats {
	uint64_t commands_translated = 0;
	uint64_t draw_commands        = 0;
	uint64_t dispatch_commands    = 0;
	uint64_t dma_commands         = 0;
	uint64_t clear_commands       = 0;
	uint64_t barrier_commands     = 0;
	uint64_t query_commands       = 0;
};

class Pm4Translator {
public:
	explicit Pm4Translator(IGraphicBackend* backend) : m_backend(backend) {}
	~Pm4Translator() = default;

	KYTY_CLASS_NO_COPY(Pm4Translator);

	void SetBackend(IGraphicBackend* backend) noexcept { m_backend = backend; }
	[[nodiscard]] IGraphicBackend* GetBackend() const noexcept { return m_backend; }

	bool TranslateAndExecute(const Pm4CommandList& cmd_list);

	[[nodiscard]] const TranslationStats& GetStats() const noexcept { return m_stats; }
	void ResetStats() noexcept { m_stats = TranslationStats{}; }

private:
	bool DispatchCommand(const GenericCommand& cmd);

	IGraphicBackend* m_backend = nullptr;
	TranslationStats m_stats{};
};

} // namespace Libs::Graphics::Pm4

#endif // GRAPHICS_GUEST_GPU_COMMAND_PROCESSOR_PM4_TRANSLATOR_H
