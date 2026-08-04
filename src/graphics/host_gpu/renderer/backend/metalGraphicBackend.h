#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/metalCommandQueue.h"
#include <cstdint>
#include <memory>

namespace Libs::Graphics {

struct MetalCapabilities {
	bool     has_unified_memory = false;
	bool     supports_argument_buffers = false;
	bool     supports_raytracing = false;
	bool     supports_barycentrics = false;
	uint32_t max_threads_per_threadgroup = 0;
	uint64_t max_buffer_length_bytes = 0;
	uint32_t max_working_set_size_mb = 0;
	char     gpu_name[256] = {};
};

class MetalCommandQueue;
class MetalPipelineCache;
class MetalArgumentBufferCache;

class MetalGraphicBackend final : public IGraphicBackend {
public:
	MetalGraphicBackend();
	~MetalGraphicBackend() override;

	[[nodiscard]] GraphicBackendType GetBackendType() const noexcept override { return GraphicBackendType::Metal; }
	[[nodiscard]] const char*        GetBackendName() const noexcept override { return "Metal"; }
	[[nodiscard]] bool               IsSupported() const noexcept override;
	[[nodiscard]] bool               Initialize() override;
	void                             Shutdown() override;
	void                             WaitIdle() override;

	[[nodiscard]] const MetalCapabilities&    GetCapabilities() const noexcept { return m_capabilities; }
	[[nodiscard]] void*                       GetMTLDevice() const noexcept { return m_device; }
	[[nodiscard]] void*                       GetMTLCommandQueue() const noexcept { return m_command_queue; }
	[[nodiscard]] uint64_t                    GetInitializationTimeNs() const noexcept { return m_init_time_ns; }

	/// Returns the Phase C command queue wrapper.  Null before Initialize().
	[[nodiscard]] MetalCommandQueue*          GetCommandQueue() noexcept { return m_metal_queue.get(); }

	/// Returns the Phase E Metal pipeline cache. Null before Initialize().
	[[nodiscard]] MetalPipelineCache*         GetPipelineCache() noexcept { return m_pipeline_cache.get(); }

	/// Returns the Phase F Metal argument buffer cache. Null before Initialize().
	[[nodiscard]] MetalArgumentBufferCache*   GetArgumentBufferCache() noexcept { return m_argument_buffer_cache.get(); }

private:
	bool                                m_initialized   = false;
	void*                               m_device        = nullptr; // id<MTLDevice>
	void*                               m_command_queue = nullptr; // id<MTLCommandQueue>
	std::unique_ptr<MetalCommandQueue>         m_metal_queue;
	std::unique_ptr<MetalPipelineCache>        m_pipeline_cache;
	std::unique_ptr<MetalArgumentBufferCache>  m_argument_buffer_cache;
	MetalCapabilities                   m_capabilities {};
	uint64_t                            m_init_time_ns  = 0;

	void QueryCapabilities();
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALGRAPHICBACKEND_H_
