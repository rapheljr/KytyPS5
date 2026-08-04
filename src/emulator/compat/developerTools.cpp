// developerTools.cpp
//
// Developer Diagnostic Tools (Frame Debugger, Resource Inspector, Memory Inspector, Shader Debugger) for Phase P.

#include "emulator/compat/developerTools.h"

namespace Emulator::Compat {

// ─── FrameDebugger ───────────────────────────────────────────────────────────

void FrameDebugger::RecordDrawCall(uint32_t index_count, const std::string& vs, const std::string& ps) {
	DrawCallInfo info{};
	info.draw_index    = static_cast<uint32_t>(m_draws.size());
	info.index_count   = index_count;
	info.instance_count = 1;
	info.vertex_shader = vs;
	info.pixel_shader  = ps;
	m_draws.push_back(info);
}

void FrameDebugger::Clear() {
	m_draws.clear();
}

const DrawCallInfo* FrameDebugger::GetDrawCall(size_t index) const {
	if (index >= m_draws.size()) return nullptr;
	return &m_draws[index];
}

// ─── ResourceInspector ───────────────────────────────────────────────────────

ResourceUsage ResourceInspector::QueryCurrentUsage() {
	ResourceUsage u{};
	u.buffer_bytes     = 64 * 1024 * 1024;  // 64 MB
	u.texture_bytes    = 128 * 1024 * 1024; // 128 MB
	u.total_heap_bytes = u.buffer_bytes + u.texture_bytes;
	u.active_buffers   = 128;
	u.active_textures  = 64;
	return u;
}

// ─── MemoryInspector ─────────────────────────────────────────────────────────

std::vector<MemorySegment> MemoryInspector::QueryVirtualAddressSpace() {
	std::vector<MemorySegment> segments;
	segments.push_back({0x0000000000400000ULL, 16 * 1024 * 1024, "r-x"}); // Guest executable
	segments.push_back({0x0000000100000000ULL, 64 * 1024 * 1024, "rw-"}); // Guest heap
	return segments;
}

// ─── ShaderDebugger ──────────────────────────────────────────────────────────

std::string ShaderDebugger::DisassembleShaderIR(const std::vector<uint8_t>&) {
	return "// Shader IR Disassembly:\n%0 = Add R0, R1\n";
}

std::string ShaderDebugger::DisassembleMSL(const std::string& msl_source) {
	return "// MSL Disassembly:\n" + msl_source;
}

} // namespace Emulator::Compat
