// developerTools.h
//
// Developer Diagnostic Tools (Frame Debugger, Resource Inspector, Memory Inspector, Shader Debugger) for Phase P.

#ifndef EMULATOR_COMPAT_DEVELOPER_TOOLS_H
#define EMULATOR_COMPAT_DEVELOPER_TOOLS_H

#include "common/common.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Emulator::Compat {

struct DrawCallInfo {
	uint32_t draw_index = 0;
	uint32_t index_count = 0;
	uint32_t instance_count = 1;
	std::string vertex_shader;
	std::string pixel_shader;
};

class FrameDebugger {
public:
	FrameDebugger() = default;
	~FrameDebugger() = default;

	KYTY_CLASS_NO_COPY(FrameDebugger);

	void RecordDrawCall(uint32_t index_count, const std::string& vs, const std::string& ps);
	void Clear();
	[[nodiscard]] size_t GetDrawCallCount() const noexcept { return m_draws.size(); }
	[[nodiscard]] const DrawCallInfo* GetDrawCall(size_t index) const;

private:
	std::vector<DrawCallInfo> m_draws;
};

struct ResourceUsage {
	size_t buffer_bytes = 0;
	size_t texture_bytes = 0;
	size_t total_heap_bytes = 0;
	uint32_t active_buffers = 0;
	uint32_t active_textures = 0;
};

class ResourceInspector {
public:
	static ResourceUsage QueryCurrentUsage();
};

struct MemorySegment {
	uint64_t vaddr = 0;
	size_t   size = 0;
	std::string protection; // e.g. "r-x", "rw-"
};

class MemoryInspector {
public:
	static std::vector<MemorySegment> QueryVirtualAddressSpace();
};

class ShaderDebugger {
public:
	static std::string DisassembleShaderIR(const std::vector<uint8_t>& ir_bytes);
	static std::string DisassembleMSL(const std::string& msl_source);
};

} // namespace Emulator::Compat

#endif // EMULATOR_COMPAT_DEVELOPER_TOOLS_H
