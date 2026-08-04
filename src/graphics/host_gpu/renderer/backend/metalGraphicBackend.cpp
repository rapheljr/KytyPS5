#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"

#if !defined(__APPLE__)

namespace Libs::Graphics {

MetalGraphicBackend::MetalGraphicBackend() = default;

MetalGraphicBackend::~MetalGraphicBackend() {
	if (m_initialized) {
		Shutdown();
	}
}

bool MetalGraphicBackend::IsSupported() const noexcept {
	return false;
}

bool MetalGraphicBackend::Initialize() {
	return false;
}

void MetalGraphicBackend::Shutdown() {}

void MetalGraphicBackend::WaitIdle() {}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
