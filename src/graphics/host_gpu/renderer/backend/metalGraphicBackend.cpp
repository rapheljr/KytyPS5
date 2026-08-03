#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"

namespace Libs::Graphics {

MetalGraphicBackend::MetalGraphicBackend() = default;

MetalGraphicBackend::~MetalGraphicBackend() {
	if (m_initialized) {
		Shutdown();
	}
}

bool MetalGraphicBackend::IsSupported() const noexcept {
#if defined(__APPLE__)
	return true;
#else
	return false;
#endif
}

bool MetalGraphicBackend::Initialize() {
	if (!IsSupported()) {
		return false;
	}
	if (m_initialized) {
		return true;
	}
	m_initialized = true;
	return true;
}

void MetalGraphicBackend::Shutdown() {
	if (!m_initialized) {
		return;
	}
	WaitIdle();
	m_initialized = false;
}

void MetalGraphicBackend::WaitIdle() {
	// Stub for Phase B Metal device waitIdle synchronization
}

} // namespace Libs::Graphics
