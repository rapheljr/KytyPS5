// metalCommandBuffer_stub.cpp
// Non-Apple stub — all methods are empty on platforms where Metal is not available.
// The header already guards all ObjC code with #if defined(__APPLE__).

#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"

#if !defined(__APPLE__)

namespace Libs::Graphics {

MetalCommandBuffer::MetalCommandBuffer(void* /*mtl_command_queue*/) {
	m_state = MetalCommandBufferState::Error;
}

MetalCommandBuffer::~MetalCommandBuffer() = default;

bool MetalCommandBuffer::IsValid() const noexcept { return false; }

MetalCommandBufferState MetalCommandBuffer::GetState() const noexcept {
	return m_state;
}

void* MetalCommandBuffer::OpenComputeEncoder() { return nullptr; }

void MetalCommandBuffer::CloseComputeEncoder() {}

void MetalCommandBuffer::Commit() {}

void MetalCommandBuffer::WaitUntilCompleted() {}

uint64_t MetalCommandBuffer::GetGpuExecutionTimeNs() const noexcept { return 0; }

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
