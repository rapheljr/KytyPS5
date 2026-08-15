// metalAsyncCompute.mm
//
// Metal Dedicated Asynchronous Compute & Multi-Queue Synchronization Engine Implementation.

#include "graphics/host_gpu/renderer/backend/metalAsyncCompute.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include <chrono>

namespace Libs::Graphics::HostGpu::Metal {

struct MetalAsyncComputeEngine::Impl {
	id<MTLDevice>               device         = nil;
	id<MTLCommandQueue>          compute_queue  = nil;
	id<MTLCommandQueue>          render_queue   = nil;
	id<MTLComputePipelineState>  pipeline_state = nil;
};

MetalAsyncComputeEngine::MetalAsyncComputeEngine() : m_impl(std::make_unique<Impl>()) {}

MetalAsyncComputeEngine::~MetalAsyncComputeEngine() {
	Shutdown();
}

bool MetalAsyncComputeEngine::Initialize() {
	@autoreleasepool {
		m_impl->device = MTLCreateSystemDefaultDevice();
		if (!m_impl->device) {
			return false;
		}

		m_impl->compute_queue = [m_impl->device newCommandQueue];
		m_impl->render_queue  = [m_impl->device newCommandQueue];

		if (!m_impl->compute_queue || !m_impl->render_queue) {
			return false;
		}

		// Compile a lightweight dummy compute kernel for testing / async work
		NSString* shaderSrc = @""
			"#include <metal_stdlib>\n"
			"using namespace metal;\n"
			"kernel void dummy_compute(uint3 id [[thread_position_in_grid]]) {\n"
			"    // Async compute pass\n"
			"}\n";

		NSError* error = nil;
		id<MTLLibrary> lib = [m_impl->device newLibraryWithSource:shaderSrc options:nil error:&error];
		if (!lib) {
			return false;
		}

		id<MTLFunction> fn = [lib newFunctionWithName:@"dummy_compute"];
		if (!fn) {
			[lib release];
			return false;
		}

		m_impl->pipeline_state = [m_impl->device newComputePipelineStateWithFunction:fn error:&error];
		[fn release];
		[lib release];

		if (!m_impl->pipeline_state) {
			return false;
		}

		m_initialized = true;
		return true;
	}
}

void MetalAsyncComputeEngine::Shutdown() {
	@autoreleasepool {
		WaitForIdle();
		if (m_impl->pipeline_state) {
			[m_impl->pipeline_state release];
			m_impl->pipeline_state = nil;
		}
		if (m_impl->compute_queue) {
			[m_impl->compute_queue release];
			m_impl->compute_queue = nil;
		}
		if (m_impl->render_queue) {
			[m_impl->render_queue release];
			m_impl->render_queue = nil;
		}
		if (m_impl->device) {
			[m_impl->device release];
			m_impl->device = nil;
		}
		m_initialized = false;
	}
}

bool MetalAsyncComputeEngine::DispatchCompute(uint32_t threadgroups_x, uint32_t threadgroups_y, uint32_t threadgroups_z,
                                              uint32_t threads_per_group_x, uint32_t threads_per_group_y, uint32_t threads_per_group_z) {
	if (!m_initialized) return false;

	@autoreleasepool {
		auto start = std::chrono::high_resolution_clock::now();

		id<MTLCommandBuffer> cmdBuf = [m_impl->compute_queue commandBuffer];
		id<MTLComputeCommandEncoder> encoder = [cmdBuf computeCommandEncoder];

		[encoder setComputePipelineState:m_impl->pipeline_state];

		MTLSize threadgroups = MTLSizeMake(threadgroups_x, threadgroups_y, threadgroups_z);
		MTLSize threadsPerGroup = MTLSizeMake(threads_per_group_x, threads_per_group_y, threads_per_group_z);

		[encoder dispatchThreadgroups:threadgroups threadsPerThreadgroup:threadsPerGroup];
		[encoder endEncoding];

		[cmdBuf commit];

		auto end = std::chrono::high_resolution_clock::now();
		m_stats.last_compute_time_ms = std::chrono::duration<double, std::milli>(end - start).count();
		m_stats.compute_dispatches_total++;

		return true;
	}
}

bool MetalAsyncComputeEngine::InsertCrossQueueBarrier() {
	if (!m_initialized) return false;

	@autoreleasepool {
		if (@available(macOS 10.14, *)) {
			id<MTLEvent> event = [m_impl->device newEvent];
			if (event) {
				id<MTLCommandBuffer> computeCmdBuf = [m_impl->compute_queue commandBuffer];
				[computeCmdBuf encodeSignalEvent:event value:1];
				[computeCmdBuf commit];

				id<MTLCommandBuffer> renderCmdBuf = [m_impl->render_queue commandBuffer];
				[renderCmdBuf encodeWaitForEvent:event value:1];
				[renderCmdBuf commit];
				[event release];

				m_stats.sync_barriers_total++;
				return true;
			}
		}

		// Fallback FIFO sync token
		id<MTLCommandBuffer> syncBuf = [m_impl->compute_queue commandBuffer];
		[syncBuf commit];
		[syncBuf waitUntilCompleted];

		m_stats.sync_barriers_total++;
		return true;
	}
}

void MetalAsyncComputeEngine::WaitForIdle() {
	if (!m_initialized) return;

	@autoreleasepool {
		if (m_impl->compute_queue) {
			id<MTLCommandBuffer> syncBuf = [m_impl->compute_queue commandBuffer];
			[syncBuf commit];
			[syncBuf waitUntilCompleted];
		}
		if (m_impl->render_queue) {
			id<MTLCommandBuffer> syncBuf = [m_impl->render_queue commandBuffer];
			[syncBuf commit];
			[syncBuf waitUntilCompleted];
		}
	}
}

} // namespace Libs::Graphics::HostGpu::Metal

#else

namespace Libs::Graphics::HostGpu::Metal {
struct MetalAsyncComputeEngine::Impl {};
MetalAsyncComputeEngine::MetalAsyncComputeEngine() : m_impl(nullptr) {}
MetalAsyncComputeEngine::~MetalAsyncComputeEngine() = default;
bool MetalAsyncComputeEngine::Initialize() { m_initialized = true; return true; }
void MetalAsyncComputeEngine::Shutdown() { m_initialized = false; }
bool MetalAsyncComputeEngine::DispatchCompute(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { return true; }
bool MetalAsyncComputeEngine::InsertCrossQueueBarrier() { return true; }
void MetalAsyncComputeEngine::WaitForIdle() {}
} // namespace Libs::Graphics::HostGpu::Metal

#endif
