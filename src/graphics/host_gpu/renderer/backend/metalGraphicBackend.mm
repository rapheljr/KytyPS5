#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "common/timer.h"

#include <chrono>
#include <cstring>

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#endif

namespace Libs::Graphics {

MetalGraphicBackend::MetalGraphicBackend() = default;

MetalGraphicBackend::~MetalGraphicBackend() {
	if (m_initialized) {
		Shutdown();
	}
}

bool MetalGraphicBackend::IsSupported() const noexcept {
#if defined(__APPLE__)
	id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
	if (dev != nil) {
		[dev release];
		return true;
	}
	return false;
#else
	return false;
#endif
}

bool MetalGraphicBackend::Initialize() {
	if (m_initialized) {
		return true;
	}

	const auto start_time = Common::Timer::QueryPerformanceCounter();

#if defined(__APPLE__)
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (device == nil) {
		m_init_time_ns = 0;
		return false;
	}

	id<MTLCommandQueue> queue = [device newCommandQueue];
	if (queue == nil) {
		[device release];
		m_init_time_ns = 0;
		return false;
	}

	m_device        = (void*)device;
	m_command_queue = (void*)queue;

	QueryCapabilities();

	const auto end_time = Common::Timer::QueryPerformanceCounter();
	const auto freq     = Common::Timer::QueryPerformanceFrequency();
	if (freq > 0) {
		m_init_time_ns = ((end_time - start_time) * 1000000000ULL) / freq;
	} else {
		m_init_time_ns = 0;
	}

	m_initialized = true;
	return true;
#else
	m_init_time_ns = 0;
	return false;
#endif
}

void MetalGraphicBackend::QueryCapabilities() {
#if defined(__APPLE__)
	if (m_device == nullptr) {
		return;
	}

	id<MTLDevice> dev = (id<MTLDevice>)m_device;
	m_capabilities    = MetalCapabilities {};

	NSString* name = [dev name];
	if (name != nil) {
		std::strncpy(m_capabilities.gpu_name, [name UTF8String], sizeof(m_capabilities.gpu_name) - 1);
	}

	if (@available(macOS 10.15, iOS 13.0, *)) {
		m_capabilities.has_unified_memory = [dev hasUnifiedMemory];
	} else {
		m_capabilities.has_unified_memory = true;
	}

	if (@available(macOS 10.15, *)) {
		m_capabilities.supports_argument_buffers =
		    ([dev argumentBuffersSupport] != MTLArgumentBuffersTier1 || [dev argumentBuffersSupport] == MTLArgumentBuffersTier2);
	} else {
		m_capabilities.supports_argument_buffers = true;
	}

	if (@available(macOS 11.0, *)) {
		m_capabilities.supports_raytracing = [dev supportsRaytracing];
	} else {
		m_capabilities.supports_raytracing = false;
	}

	if (@available(macOS 10.15, *)) {
		m_capabilities.supports_barycentrics = [dev supportsShaderBarycentricCoordinates];
	} else {
		m_capabilities.supports_barycentrics = false;
	}

	MTLSize threads                            = [dev maxThreadsPerThreadgroup];
	m_capabilities.max_threads_per_threadgroup = static_cast<uint32_t>(threads.width * threads.height * threads.depth);

	if (@available(macOS 10.14, *)) {
		m_capabilities.max_buffer_length_bytes = [dev maxBufferLength];
	} else {
		m_capabilities.max_buffer_length_bytes = 256 * 1024 * 1024;
	}

	if (@available(macOS 10.12, *)) {
		m_capabilities.max_working_set_size_mb = static_cast<uint32_t>([dev recommendedMaxWorkingSetSize] / (1024 * 1024));
	}
#endif
}

void MetalGraphicBackend::Shutdown() {
	if (!m_initialized) {
		return;
	}
	WaitIdle();

#if defined(__APPLE__)
	if (m_command_queue != nullptr) {
		id<MTLCommandQueue> queue = (id<MTLCommandQueue>)m_command_queue;
		[queue release];
		m_command_queue = nullptr;
	}
	if (m_device != nullptr) {
		id<MTLDevice> dev = (id<MTLDevice>)m_device;
		[dev release];
		m_device = nullptr;
	}
#endif

	m_capabilities = MetalCapabilities {};
	m_initialized  = false;
}

void MetalGraphicBackend::WaitIdle() {
	// Stub for Phase B queue synchronization
}

} // namespace Libs::Graphics
