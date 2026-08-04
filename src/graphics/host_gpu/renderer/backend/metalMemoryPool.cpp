#include "graphics/host_gpu/renderer/backend/metalMemoryPool.h"

#if !defined(__APPLE__)

#include <algorithm>

namespace Libs::Graphics {

MetalGpuHeapAllocator::~MetalGpuHeapAllocator() {
	Shutdown();
}

bool MetalGpuHeapAllocator::Initialize(void* /*mtl_device*/, uint64_t /*initial_heap_size_bytes*/) {
	return false;
}

void MetalGpuHeapAllocator::Shutdown() {
	m_heaps.clear();
	m_stats.Reset();
}

void* MetalGpuHeapAllocator::AllocateBuffer(uint64_t /*size_bytes*/, MetalBufferMemoryType /*mem_type*/) {
	return nullptr;
}

void* MetalGpuHeapAllocator::AllocateTexture(void* /*mtl_texture_desc*/) {
	return nullptr;
}

uint64_t MetalGpuHeapAllocator::GetTotalHeapBytes() const noexcept {
	return 0;
}

size_t MetalGpuHeapAllocator::GetHeapCount() const noexcept {
	return 0;
}

MetalResourceDeferrer::~MetalResourceDeferrer() {
	Shutdown();
}

void MetalResourceDeferrer::Initialize() {
	Shutdown();
}

void MetalResourceDeferrer::Shutdown() {
	m_pending_queue.clear();
}

void MetalResourceDeferrer::DeferRelease(void* /*mtl_resource*/, uint64_t /*frame_index*/) {
}

void MetalResourceDeferrer::ProcessDeferredReleases(uint64_t /*completed_frame_index*/) {
}

size_t MetalResourceDeferrer::GetPendingReleaseCount() const noexcept {
	return 0;
}

MetalUploadStaging::~MetalUploadStaging() {
	Shutdown();
}

bool MetalUploadStaging::Initialize(void* /*mtl_device*/, uint64_t /*default_capacity_bytes*/) {
	return false;
}

void MetalUploadStaging::Shutdown() {
	m_staging_buffer.Destroy();
	m_head = 0;
}

MetalUploadStaging::StagingAllocation MetalUploadStaging::StageUpload(const void* /*src_bytes*/, uint64_t /*size_bytes*/, uint64_t /*alignment*/) {
	return {};
}

void MetalUploadStaging::ResetPool() {
	m_head = 0;
}

MetalReadbackStaging::~MetalReadbackStaging() {
	Shutdown();
}

bool MetalReadbackStaging::Initialize(void* /*mtl_device*/, uint64_t /*capacity_bytes*/) {
	return false;
}

void MetalReadbackStaging::Shutdown() {
	m_readback_buffer.Destroy();
	m_head = 0;
}

MetalReadbackStaging::ReadbackAllocation MetalReadbackStaging::AllocateReadback(uint64_t /*size_bytes*/, uint64_t /*alignment*/) {
	return {};
}

void MetalReadbackStaging::ResetPool() {
	m_head = 0;
}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
