#pragma once

#include <cstdint>
#include <vector>
#include <memory>

namespace HostGpu::Backend {

enum class IndirectCommandType {
	Draw = 0,
	DrawIndexed = 1,
	DrawPatches = 2,
	DrawIndexedPatches = 3
};

struct IndirectDrawCommandItem {
	uint32_t command_index = 0;
	IndirectCommandType type = IndirectCommandType::DrawIndexed;
	uint32_t primitive_type = 3; // MTLPrimitiveTypeTriangle = 3
	uint32_t vertex_count = 0;
	uint32_t index_count = 0;
	uint32_t instance_count = 1;
	uint32_t vertex_start = 0;
	uint32_t index_start = 0;
	uint32_t base_vertex = 0;
	uint32_t base_instance = 0;
	uint64_t index_buffer_gpu_address = 0;
};

struct MetalIcbConfig {
	uint32_t max_command_count = 4096;
	bool inherit_pipeline_state = true;
	bool inherit_buffers = true;
	uint32_t max_vertex_buffers = 8;
	uint32_t max_fragment_buffers = 8;
};

class MetalIndirectCommandBuffer {
public:
	MetalIndirectCommandBuffer();
	~MetalIndirectCommandBuffer();

	bool Initialize(const MetalIcbConfig& config);
	void Shutdown();

	bool IsInitialized() const { return m_initialized; }
	uint32_t GetMaxCommandCount() const { return m_config.max_command_count; }
	uint32_t GetRecordedCommandCount() const { return static_cast<uint32_t>(m_commands.size()); }

	bool SetDrawIndexed(uint32_t index, uint32_t index_count, uint32_t instance_count,
	                    uint32_t index_start, int32_t base_vertex, uint32_t base_instance);
	bool SetDraw(uint32_t index, uint32_t vertex_count, uint32_t instance_count,
	             uint32_t vertex_start, uint32_t base_instance);

	void Reset();

	// Execute recorded ICB in render command encoder
	bool Execute(void* render_command_encoder);

	void* GetNativeHandle() const { return m_native_icb; }

private:
	MetalIcbConfig m_config;
	bool m_initialized = false;
	void* m_native_icb = nullptr;
	std::vector<IndirectDrawCommandItem> m_commands;
};

} // namespace HostGpu::Backend
