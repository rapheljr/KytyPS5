#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALBUFFER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALBUFFER_H_

#include "common/common.h"

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>

namespace Libs::Graphics {

enum class MetalBufferUsage : uint32_t {
	None        = 0,
	Vertex      = 1 << 0,
	Index       = 1 << 1,
	Uniform     = 1 << 2,
	Storage     = 1 << 3,
	Upload      = 1 << 4,
	Readback    = 1 << 5,
	Dynamic     = 1 << 6,
	Indirect    = 1 << 7
};

inline MetalBufferUsage operator|(MetalBufferUsage a, MetalBufferUsage b) {
	return static_cast<MetalBufferUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline bool HasUsage(MetalBufferUsage usage, MetalBufferUsage flag) {
	return (static_cast<uint32_t>(usage) & static_cast<uint32_t>(flag)) != 0;
}

enum class MetalBufferMemoryType : uint8_t {
	Shared,   // MTLStorageModeShared (Apple Silicon Unified Memory - CPU/GPU shared)
	Private,  // MTLStorageModePrivate (GPU VRAM high bandwidth)
	Managed,  // MTLStorageModeManaged (Intel macOS)
	Staging   // CPU staging memory
};

class MetalBuffer {
public:
	MetalBuffer() = default;
	~MetalBuffer();

	MetalBuffer(const MetalBuffer&) = delete;
	MetalBuffer& operator=(const MetalBuffer&) = delete;

	MetalBuffer(MetalBuffer&& other) noexcept;
	MetalBuffer& operator=(MetalBuffer&& other) noexcept;

	bool Create(void* mtl_device, uint64_t size_bytes, MetalBufferUsage usage, MetalBufferMemoryType mem_type, const void* initial_data = nullptr);
	void Destroy();

	[[nodiscard]] void*                 GetMTLBuffer() const noexcept { return m_buffer; }
	[[nodiscard]] uint64_t              GetSize() const noexcept { return m_size; }
	[[nodiscard]] MetalBufferUsage      GetUsage() const noexcept { return m_usage; }
	[[nodiscard]] MetalBufferMemoryType GetMemoryType() const noexcept { return m_mem_type; }
	[[nodiscard]] void*                 GetMappedPointer() const noexcept { return m_mapped_ptr; }
	[[nodiscard]] bool                  IsMapped() const noexcept { return m_mapped_ptr != nullptr; }

	void* Map();
	void  Unmap();
	bool  Write(const void* src, uint64_t size, uint64_t offset = 0);
	bool  Read(void* dst, uint64_t size, uint64_t offset = 0);
	void  Flush(uint64_t offset = 0, uint64_t size = 0);

private:
	void*                 m_buffer     = nullptr; // id<MTLBuffer>
	uint64_t              m_size       = 0;
	MetalBufferUsage      m_usage      = MetalBufferUsage::None;
	MetalBufferMemoryType m_mem_type   = MetalBufferMemoryType::Shared;
	void*                 m_mapped_ptr = nullptr;
};

class MetalRingBuffer {
public:
	MetalRingBuffer() = default;
	~MetalRingBuffer();

	bool Initialize(void* mtl_device, uint64_t capacity_bytes, MetalBufferUsage usage = MetalBufferUsage::Uniform);
	void Shutdown();

	struct Allocation {
		void*    mtl_buffer   = nullptr;
		void*    mapped_ptr   = nullptr;
		uint64_t offset_bytes = 0;
		uint64_t size_bytes   = 0;
		bool     valid        = false;
	};

	[[nodiscard]] Allocation Allocate(uint64_t size_bytes, uint64_t alignment = 256);
	void Reset();

	[[nodiscard]] uint64_t GetCapacity() const noexcept { return m_capacity; }
	[[nodiscard]] uint64_t GetHeadOffset() const noexcept { return m_head.load(std::memory_order_relaxed); }
	[[nodiscard]] uint64_t GetAllocatedBytes() const noexcept { return m_allocated_bytes.load(std::memory_order_relaxed); }

private:
	MetalBuffer              m_buffer;
	uint64_t                 m_capacity = 0;
	std::atomic<uint64_t>    m_head {0};
	std::atomic<uint64_t>    m_allocated_bytes {0};
	mutable std::mutex       m_mutex;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALBUFFER_H_
