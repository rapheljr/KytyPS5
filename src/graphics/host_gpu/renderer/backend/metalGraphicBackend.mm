#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/metalArgumentBuffer.h"
#include "graphics/host_gpu/renderer/backend/metalCommandQueue.h"
#include "graphics/host_gpu/renderer/backend/metalPipelineCache.h"
#include "graphics/host_gpu/renderer/backend/metalSync.h"
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

	// Phase C: construct the command queue C++ wrapper
	m_metal_queue = std::make_unique<MetalCommandQueue>(m_command_queue);

	// Phase E: construct the Metal pipeline cache C++ wrapper
	m_pipeline_cache = std::make_unique<MetalPipelineCache>(m_device);

	// Phase F: construct the Metal argument buffer cache C++ wrapper
	m_argument_buffer_cache = std::make_unique<MetalArgumentBufferCache>(m_device);

	// Phase H: construct Metal frame sync and hazard tracker
	m_frame_sync     = std::make_unique<HostGpu::Metal::MetalFrameSync>(3);
	m_hazard_tracker = std::make_unique<HostGpu::Metal::MetalResourceHazardTracker>();

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

	m_hazard_tracker.reset();
	m_frame_sync.reset();
	m_argument_buffer_cache.reset();
	m_pipeline_cache.reset();
	m_metal_queue.reset();

	m_capabilities = MetalCapabilities {};
	m_initialized  = false;
}

void MetalGraphicBackend::WaitIdle() {
	if (m_metal_queue != nullptr) {
		m_metal_queue->WaitAllCompleted();
	}
}

bool MetalGraphicBackend::StartCapture(const char* capture_path) noexcept {
#if defined(__APPLE__)
	if (!m_initialized || m_capturing || m_device == nullptr) {
		return false;
	}

	MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
	if (!mgr || ![mgr supportsDestination:MTLCaptureDestinationGPUTraceDocument]) {
		// Fallback to Xcode/developer tools capture if file destination is unsupported
		MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor alloc] init];
		desc.captureObject = (__bridge id<MTLDevice>)m_device;
		desc.destination   = MTLCaptureDestinationDeveloperTools;
		NSError* err = nil;
		if ([mgr startCaptureWithDescriptor:desc error:&err]) {
			m_capturing = true;
			return true;
		}
		return false;
	}

	MTLCaptureDescriptor* desc = [[MTLCaptureDescriptor alloc] init];
	desc.captureObject = (__bridge id<MTLDevice>)m_device;
	desc.destination   = MTLCaptureDestinationGPUTraceDocument;

	if (capture_path != nullptr) {
		desc.outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:capture_path]];
	} else {
		NSString* default_path = [NSTemporaryDirectory() stringByAppendingPathComponent:@"kyty_metal_capture.gputrace"];
		desc.outputURL = [NSURL fileURLWithPath:default_path];
	}

	NSError* err = nil;
	if ([mgr startCaptureWithDescriptor:desc error:&err]) {
		m_capturing = true;
		return true;
	}
#else
	(void)capture_path;
#endif
	return false;
}

void MetalGraphicBackend::StopCapture() noexcept {
#if defined(__APPLE__)
	if (m_capturing) {
		MTLCaptureManager* mgr = [MTLCaptureManager sharedCaptureManager];
		if (mgr) {
			[mgr stopCapture];
		}
		m_capturing = false;
	}
#endif
}

MetalCommandBuffer* MetalGraphicBackend::AcquireCurrentCommandBuffer() {
#if defined(__APPLE__)
	if (!m_initialized || !m_metal_queue) {
		return nullptr;
	}
	if (!m_active_cmd_buf || m_active_cmd_buf->GetState() != MetalCommandBufferState::Recording) {
		m_active_cmd_buf = m_metal_queue->CreateCommandBuffer();
	}
	return m_active_cmd_buf.get();
#else
	return nullptr;
#endif
}

bool MetalGraphicBackend::BeginRenderPass(uint32_t width, uint32_t height,
                                         float r, float g, float b, float a) {
#if defined(__APPLE__)
	auto* cb = AcquireCurrentCommandBuffer();
	if (!cb) return false;

	// Invalidate any currently open render encoder before rebinding attachment state
	cb->CloseRenderEncoder();

	MTLRenderPassDescriptor* rpd = [MTLRenderPassDescriptor renderPassDescriptor];
	rpd.colorAttachments[0].loadAction  = MTLLoadActionClear;
	rpd.colorAttachments[0].storeAction = MTLStoreActionStore;
	rpd.colorAttachments[0].clearColor  = MTLClearColorMake(r, g, b, a);

	void* enc = cb->OpenRenderEncoder((__bridge void*)rpd);
	if (!enc) return false;

	cb->SetViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
	cb->SetScissorRect(0, 0, width, height);
	return true;
#else
	(void)width; (void)height; (void)r; (void)g; (void)b; (void)a;
	return false;
#endif
}

void MetalGraphicBackend::EndRenderPass() {
	if (m_active_cmd_buf) {
		m_active_cmd_buf->CloseRenderEncoder();
	}
}

bool MetalGraphicBackend::SubmitCurrentCommandBuffer() {
	if (!m_active_cmd_buf || m_active_cmd_buf->GetState() != MetalCommandBufferState::Recording) {
		return false;
	}
	m_active_cmd_buf->Commit();
	return true;
}

void MetalGraphicBackend::PresentFrame(uint32_t buffer_index) {
#if defined(__APPLE__)
	if (m_frame_sync) {
		m_frame_sync->BeginFrame();
	}
	if (m_active_cmd_buf && m_active_cmd_buf->GetState() == MetalCommandBufferState::Recording) {
		EndRenderPass();
		if (m_frame_sync) {
			m_frame_sync->EndFrame(m_active_cmd_buf->GetNativeCommandBuffer());
		}
		SubmitCurrentCommandBuffer();
	} else if (m_frame_sync) {
		m_frame_sync->EndFrame(nullptr);
	}
#else
	(void)buffer_index;
#endif
}

} // namespace Libs::Graphics
