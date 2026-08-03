// GraphicBackendTests.cpp
//
// Unit tests verifying Phase A, B & C Graphic Backend Interface, Factory
// runtime selection, Metal Device Initialization, Command Queue Wrapper,
// Command Buffer Lifecycle, and Synchronization.

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"
#include "graphics/host_gpu/renderer/backend/metalCommandQueue.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, const char* text) {
	if (!value) {
		std::fprintf(stderr, "GraphicBackendTests: failed: %s\n", text);
		std::abort();
	}
}

// ─── Phase A & B ─────────────────────────────────────────────────────────────

void TestDefaultBackend() {
	auto default_type = Libs::Graphics::GraphicBackendFactory::GetDefaultBackendType();
	Check(default_type == Libs::Graphics::GraphicBackendType::Vulkan, "Default backend must be Vulkan");
}

void TestVulkanBackendCreation() {
	auto backend = Libs::Graphics::GraphicBackendFactory::CreateBackend(Libs::Graphics::GraphicBackendType::Vulkan);
	Check(backend != nullptr, "Vulkan backend creation failed");
	Check(backend->GetBackendType() == Libs::Graphics::GraphicBackendType::Vulkan, "Backend type mismatch");
	Check(std::strcmp(backend->GetBackendName(), "Vulkan") == 0, "Backend name mismatch");
	Check(backend->IsSupported(), "Vulkan backend must be supported");

	bool init_ok = backend->Initialize();
	Check(init_ok, "Vulkan backend initialization failed");
	backend->Shutdown();
}

void TestMetalBackendCreation() {
	auto backend = Libs::Graphics::GraphicBackendFactory::CreateBackend(Libs::Graphics::GraphicBackendType::Metal);
	Check(backend != nullptr, "Metal backend creation failed");
	Check(backend->GetBackendType() == Libs::Graphics::GraphicBackendType::Metal, "Backend type mismatch");
	Check(std::strcmp(backend->GetBackendName(), "Metal") == 0, "Backend name mismatch");

#if defined(__APPLE__)
	Check(backend->IsSupported(), "Metal backend must be supported on Apple platforms");
	bool init_ok = backend->Initialize();
	Check(init_ok, "Metal backend initialization failed");
	backend->Shutdown();
#else
	Check(!backend->IsSupported(), "Metal backend should not be supported on non-Apple platforms");
#endif
}

void TestMetalDeviceInitAndCapabilities() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.IsSupported(), "Metal must be supported on Apple host");
	Check(metal_backend.Initialize(), "Metal device & queue initialization failed");

	Check(metal_backend.GetMTLDevice() != nullptr, "MTLDevice handle must not be null");
	Check(metal_backend.GetMTLCommandQueue() != nullptr, "MTLCommandQueue handle must not be null");

	const auto& caps = metal_backend.GetCapabilities();
	Check(std::strlen(caps.gpu_name) > 0, "Metal GPU device name must not be empty");
	Check(caps.max_threads_per_threadgroup > 0, "Max threads per threadgroup must be > 0");
	Check(caps.max_buffer_length_bytes > 0, "Max buffer length bytes must be > 0");

	std::printf("  [Metal Info] GPU Name: %s\n", caps.gpu_name);
	std::printf("  [Metal Info] Unified Memory: %s\n", caps.has_unified_memory ? "Yes" : "No");
	std::printf("  [Metal Info] Argument Buffers Tier 2+: %s\n", caps.supports_argument_buffers ? "Yes" : "No");
	std::printf("  [Metal Info] Raytracing Support: %s\n", caps.supports_raytracing ? "Yes" : "No");
	std::printf("  [Metal Info] Max Threads / Threadgroup: %u\n", caps.max_threads_per_threadgroup);
	std::printf("  [Metal Info] Max Buffer Size: %llu MB\n", static_cast<unsigned long long>(caps.max_buffer_length_bytes / (1024 * 1024)));

	metal_backend.Shutdown();
	Check(metal_backend.GetMTLDevice() == nullptr, "MTLDevice handle must be null after shutdown");
	Check(metal_backend.GetMTLCommandQueue() == nullptr, "MTLCommandQueue handle must be null after shutdown");
#endif
}

void TestUnsupportedFallback() {
	Libs::Graphics::MetalGraphicBackend metal_backend;
#if !defined(__APPLE__)
	Check(!metal_backend.Initialize(), "Metal initialization must fail gracefully on non-Apple platform");
#else
	metal_backend.Shutdown();
	Check(metal_backend.GetMTLDevice() == nullptr, "MTLDevice must be null after shutdown");
#endif
}

void TestInitializationBenchmarking() {
	Libs::Graphics::VulkanGraphicBackend vulkan_backend;
	bool vk_ok = vulkan_backend.Initialize();
	Check(vk_ok, "Vulkan initialization benchmark failed");
	vulkan_backend.Shutdown();

#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	bool metal_ok = metal_backend.Initialize();
	Check(metal_ok, "Metal initialization benchmark failed");

	uint64_t init_ns = metal_backend.GetInitializationTimeNs();
	std::printf("  [Benchmark] Metal Device Init Latency: %llu ns (%.3f ms)\n",
	            static_cast<unsigned long long>(init_ns), static_cast<double>(init_ns) / 1e6);
	metal_backend.Shutdown();
#endif
}

// ─── Phase C: Command Queue & Buffer Tests ────────────────────────────────────

void TestPhaseC_CommandQueueWrapperCreation() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init for Phase C failed");

	Libs::Graphics::MetalCommandQueue* queue = backend.GetCommandQueue();
	Check(queue != nullptr, "GetCommandQueue() must not be null after Initialize()");
	Check(queue->IsValid(), "MetalCommandQueue must be valid");
	Check(queue->GetMTLCommandQueue() != nullptr, "Raw MTLCommandQueue handle must not be null");
	Check(queue->GetTotalCommandBuffersCreated() == 0, "Newly created queue counter must be 0");

	std::printf("  [OK] Phase C: MetalCommandQueue wrapper created\n");
	backend.Shutdown();
#else
	std::printf("  [SKIP] Phase C: TestPhaseC_CommandQueueWrapperCreation (non-Apple)\n");
#endif
}

void TestPhaseC_CommandBufferLifecycle() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");
	Libs::Graphics::MetalCommandQueue* queue = backend.GetCommandQueue();

	// Create buffer → Recording state
	auto buf = queue->CreateCommandBuffer();
	Check(buf != nullptr, "CreateCommandBuffer() must not return null");
	Check(buf->IsValid(), "Command buffer must be valid");
	Check(buf->GetState() == Libs::Graphics::MetalCommandBufferState::Recording,
	      "Newly created command buffer must be in Recording state");
	Check(queue->GetTotalCommandBuffersCreated() == 1, "Created counter must be 1");

	// Commit → Committed state
	buf->Commit();
	Check(buf->GetState() == Libs::Graphics::MetalCommandBufferState::Committed,
	      "After Commit(), state must be Committed");

	// Wait → Completed state
	buf->WaitUntilCompleted();
	Check(buf->GetState() == Libs::Graphics::MetalCommandBufferState::Completed,
	      "After WaitUntilCompleted(), state must be Completed");

	std::printf("  [OK] Phase C: Command buffer lifecycle (Recording → Committed → Completed)\n");
	backend.Shutdown();
#else
	std::printf("  [SKIP] Phase C: TestPhaseC_CommandBufferLifecycle (non-Apple)\n");
#endif
}

void TestPhaseC_ComputeEncoderScope() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");
	Libs::Graphics::MetalCommandQueue* queue = backend.GetCommandQueue();

	auto buf = queue->CreateCommandBuffer();
	Check(buf != nullptr, "Command buffer must not be null");

	// Open compute encoder
	void* enc = buf->OpenComputeEncoder();
	Check(enc != nullptr, "OpenComputeEncoder() must return a valid encoder");

	// Cannot open a second encoder while one is open
	void* enc2 = buf->OpenComputeEncoder();
	Check(enc2 == nullptr, "Second OpenComputeEncoder() must return null while encoder is active");

	// Close encoder
	buf->CloseComputeEncoder();

	// Commit and wait
	buf->Commit();
	buf->WaitUntilCompleted();
	Check(buf->GetState() == Libs::Graphics::MetalCommandBufferState::Completed,
	      "Buffer with compute encoder must reach Completed state");

	std::printf("  [OK] Phase C: Compute encoder open/close/commit\n");
	backend.Shutdown();
#else
	std::printf("  [SKIP] Phase C: TestPhaseC_ComputeEncoderScope (non-Apple)\n");
#endif
}

void TestPhaseC_WaitAllCompleted() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");
	Libs::Graphics::MetalCommandQueue* queue = backend.GetCommandQueue();

	// Submit several empty buffers without waiting on each individually
	static constexpr int N = 5;
	for (int i = 0; i < N; ++i) {
		auto buf = queue->CreateCommandBuffer();
		Check(buf != nullptr, "Batch command buffer must not be null");
		buf->Commit();
		// Buffer destructor calls WaitUntilCompleted() automatically for Committed+not-waited
	}
	Check(queue->GetTotalCommandBuffersCreated() == static_cast<uint64_t>(N),
	      "Total created buffers must match submission count");

	// Queue-level drain
	queue->WaitAllCompleted();

	std::printf("  [OK] Phase C: WaitAllCompleted queue drain (%d buffers)\n", N);
	backend.Shutdown();
#else
	std::printf("  [SKIP] Phase C: TestPhaseC_WaitAllCompleted (non-Apple)\n");
#endif
}

void TestPhaseC_VulkanBackendUnchanged() {
	// Verify Vulkan backend still initializes and shuts down correctly
	Libs::Graphics::VulkanGraphicBackend vk;
	Check(vk.Initialize(), "Vulkan backend must still initialize after Phase C changes");
	vk.WaitIdle();
	vk.Shutdown();
	std::printf("  [OK] Phase C: Vulkan backend unchanged\n");
}

void BenchmarkPhaseC_QueueOverhead() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed for bench");
	Libs::Graphics::MetalCommandQueue* queue = backend.GetCommandQueue();

	static constexpr size_t ITERS = 1000;
	const auto t0 = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERS; ++i) {
		auto buf = queue->CreateCommandBuffer();
		buf->Commit();
		buf->WaitUntilCompleted();
	}
	const auto t1 = std::chrono::high_resolution_clock::now();
	double total_ms = static_cast<double>(
	    std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	double per_buf_us = (total_ms * 1000.0) / static_cast<double>(ITERS);

	std::printf("  [Bench] MetalCommandBuffer round-trip: %.2f µs/buffer "
	            "(%.2f ms total, %zu buffers)\n",
	            per_buf_us, total_ms, ITERS);

	backend.Shutdown();
#else
	std::printf("  [SKIP] BenchmarkPhaseC_QueueOverhead (non-Apple)\n");
#endif
}

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Graphic Backend Interface Tests            \n");
	std::printf("====================================================\n\n");

	// Phase A & B
	TestDefaultBackend();
	TestVulkanBackendCreation();
	TestMetalBackendCreation();
	TestMetalDeviceInitAndCapabilities();
	TestUnsupportedFallback();
	TestInitializationBenchmarking();

	std::printf("\n--- Phase C: Command Queue & Command Buffer ---\n\n");

	// Phase C
	TestPhaseC_CommandQueueWrapperCreation();
	TestPhaseC_CommandBufferLifecycle();
	TestPhaseC_ComputeEncoderScope();
	TestPhaseC_WaitAllCompleted();
	TestPhaseC_VulkanBackendUnchanged();

	std::printf("\n");
	BenchmarkPhaseC_QueueOverhead();

	std::printf("\nGraphicBackendTests: PASSED\n");
	return 0;
}

