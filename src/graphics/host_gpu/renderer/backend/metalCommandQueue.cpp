#include "graphics/host_gpu/renderer/backend/metalCommandQueue.h"

#include <utility>

namespace Libs::Graphics {

MetalCommandQueue::MetalCommandQueue(void* mtl_command_queue) noexcept
    : m_queue(mtl_command_queue) {}

MetalCommandQueue::~MetalCommandQueue() {
	// We do not own the MTLCommandQueue; MetalGraphicBackend owns it.
	m_queue = nullptr;
}

MetalCommandQueue::MetalCommandQueue(MetalCommandQueue&& other) noexcept
    : m_queue(std::exchange(other.m_queue, nullptr)),
      m_total_created(std::exchange(other.m_total_created, 0)),
      m_total_completed(std::exchange(other.m_total_completed, 0)) {}

MetalCommandQueue& MetalCommandQueue::operator=(MetalCommandQueue&& other) noexcept {
	if (this != &other) {
		m_queue           = std::exchange(other.m_queue, nullptr);
		m_total_created   = std::exchange(other.m_total_created, 0);
		m_total_completed = std::exchange(other.m_total_completed, 0);
	}
	return *this;
}

std::unique_ptr<MetalCommandBuffer> MetalCommandQueue::CreateCommandBuffer() {
	if (m_queue == nullptr) {
		return nullptr;
	}
	auto buf = std::make_unique<MetalCommandBuffer>(m_queue);
	if (!buf->IsValid()) {
		return nullptr;
	}
	++m_total_created;
	return buf;
}

void MetalCommandQueue::WaitAllCompleted() {
	// MetalCommandBuffer::WaitUntilCompleted() is called by callers after
	// Commit().  This method provides a single-call drain for all buffers
	// that were submitted via Commit() but not yet waited on.
	//
	// Because MetalCommandBuffer is the unit that tracks in-flight state, and
	// callers hold the unique_ptr, this method inserts a lightweight
	// MTLCommandQueue-level fence: submit a no-op command buffer and wait on it.
	// This guarantees all previously committed work on this queue is retired.
#if defined(__APPLE__)
	if (m_queue == nullptr) {
		return;
	}
	auto fence = std::make_unique<MetalCommandBuffer>(m_queue);
	if (fence->IsValid()) {
		fence->Commit();
		fence->WaitUntilCompleted();
		++m_total_completed;
	}
#endif
}

} // namespace Libs::Graphics
