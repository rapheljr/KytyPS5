#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALMEMORYPOOL_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALMEMORYPOOL_H_

#include "common/common.h"
#include "graphics/host_gpu/renderer/backend/metalBuffer.h"

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>

namespace Libs::Graphics {

struct MetalGpuMemoryStats {
	std::atomic<uint64_t> allocated_bytes {0};
	std::atomic<uint64_t> used_bytes {0};
	std::atomic<uint64_t> peak_bytes {0};
	std::atomic<uint32_t> buffer_count {0};
	std::atomic<uint32_t> texture_count {0};
	std::atomic<uint32_t> heap_count {0};

	void RecordAllocation(uint64_t size_bytes, bool is_texture = false);
	void RecordDeallocation(uint64_t size_bytes, bool is_texture = false);
	void Reset();
};

class MetalGpuHeapAllocator {
public:
	MetalGpuHeapAllocator() = default;
	~MetalGpuHeapAllocator();

	MetalGpuHeapAllocator(const MetalGpuHeapAllocator&) = delete;
	MetalGpuHeapAllocator& operator=(const MetalGpuHeapAllocator&) = delete;

	bool Initialize(void* mtl_device, uint64_t initial_heap_size_bytes = 16 * 1024 * 1024);
	void Shutdown();

	[[nodiscard]] void* AllocateBuffer(uint64_t size_bytes, MetalBufferMemoryType mem_type = MetalBufferMemoryType::Private);
	[[nodiscard]] void* AllocateTexture(void* mtl_texture_desc);

	[[nodiscard]] uint64_t GetTotalHeapBytes() const noexcept;
	[[nodiscard]] size_t   GetHeapCount() const noexcept;

private:
	void*                     m_mtl_device = nullptr;
	std::vector<void*>        m_heaps; // id<MTLHeap>
	mutable std::mutex        m_mutex;
	MetalGpuMemoryStats       m_stats;
};

class MetalResourceDeferrer {
public:
	MetalResourceDeferrer() = default;
	~MetalResourceDeferrer();

	void Initialize();
	void Shutdown();

	void DeferRelease(void* mtl_resource, uint64_t frame_index);
	void ProcessDeferredReleases(uint64_t completed_frame_index);

	[[nodiscard]] size_t GetPendingReleaseCount() const noexcept;

private:
	struct PendingRelease {
		void*    resource = nullptr;
		uint64_t frame_index = 0;
	};

	std::vector<PendingRelease> m_pending_queue;
	mutable std::mutex          m_mutex;
};

class MetalUploadStaging {
public:
	MetalUploadStaging() = default;
	~MetalUploadStaging();

	bool Initialize(void* mtl_device, uint64_t default_capacity_bytes = 8 * 1024 * 1024);
	void Shutdown();

	struct StagingAllocation {
		MetalBuffer* buffer     = nullptr;
		void*        mapped_ptr = nullptr;
		uint64_t     offset     = 0;
		uint64_t     size       = 0;
		bool         valid      = false;
	};

	[[nodiscard]] StagingAllocation StageUpload(const void* src_bytes, uint64_t size_bytes, uint64_t alignment = 256);
	void ResetPool();

private:
	void*               m_mtl_device = nullptr;
	MetalBuffer         m_staging_buffer;
	std::atomic<uint64_t> m_head {0};
	std::mutex          m_mutex;
};

class MetalReadbackStaging {
public:
	MetalReadbackStaging() = default;
	~MetalReadbackStaging();

	bool Initialize(void* mtl_device, uint64_t capacity_bytes = 4 * 1024 * 1024);
	void Shutdown();

	struct ReadbackAllocation {
		MetalBuffer* buffer     = nullptr;
		void*        mapped_ptr = nullptr;
		uint64_t     offset     = 0;
		uint64_t     size       = 0;
		bool         valid      = false;
	};

	[[nodiscard]] ReadbackAllocation AllocateReadback(uint64_t size_bytes, uint64_t alignment = 256);
	void ResetPool();

private:
	void*                 m_mtl_device = nullptr;
	MetalBuffer           m_readback_buffer;
	std::atomic<uint64_t> m_head {0};
	std::mutex            m_mutex;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALMEMORYPOOL_H_
