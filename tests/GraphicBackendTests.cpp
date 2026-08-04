// GraphicBackendTests.cpp
//
// Unit tests verifying Phase A, B & C Graphic Backend Interface, Factory
// runtime selection, Metal Device Initialization, Command Queue Wrapper,
// Command Buffer Lifecycle, and Synchronization.

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalArgumentBuffer.h"
#include "graphics/host_gpu/renderer/backend/metalCommandBuffer.h"
#include "graphics/host_gpu/renderer/backend/metalCommandQueue.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/metalPipelineCache.h"
#include "graphics/host_gpu/renderer/backend/metalSwapchain.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

#include "SDL.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

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

// ─── Phase D: CAMetalLayer Integration & Swapchain Tests ───────────────────────

void TestPhaseD_AttachDetach() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init for Phase D failed");

	SDL_Window* window = SDL_CreateWindow("TestPhaseD_AttachDetach",
	                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	Check(window != nullptr, "SDL_CreateWindow failed");

	Libs::Graphics::MetalSwapchain swapchain;
	Check(!swapchain.IsAttached(), "Newly constructed swapchain must not be attached");

	bool attach_ok = swapchain.Attach(window, backend.GetMTLDevice());
	Check(attach_ok, "MetalSwapchain::Attach failed");
	Check(swapchain.IsAttached(), "MetalSwapchain must be attached");

	Check(swapchain.GetDrawableWidth() >= 800, "Drawable width must be >= 800");
	Check(swapchain.GetDrawableHeight() >= 600, "Drawable height must be >= 600");

	swapchain.Detach();
	Check(!swapchain.IsAttached(), "Swapchain must not be attached after Detach");

	SDL_DestroyWindow(window);
	backend.Shutdown();
	std::printf("  [OK] Phase D: CAMetalLayer Attach & Detach\n");
#else
	std::printf("  [SKIP] Phase D: TestPhaseD_AttachDetach (non-Apple)\n");
#endif
}

void TestPhaseD_ResizeAndDrawableRecreation() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");

	SDL_Window* window = SDL_CreateWindow("TestPhaseD_Resize",
	                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	Check(window != nullptr, "SDL_CreateWindow failed");

	Libs::Graphics::MetalSwapchain swapchain;
	Check(swapchain.Attach(window, backend.GetMTLDevice()), "Attach failed");

	// Initial acquisition
	auto frame1 = swapchain.AcquireDrawable();
	Check(frame1.drawable != nullptr, "AcquireDrawable failed on initial 800x600");
	Check(frame1.texture != nullptr, "Drawable texture must not be null");
	Check(frame1.width >= 800, "Frame 1 width must be >= 800");
	Check(frame1.height >= 600, "Frame 1 height must be >= 600");

	// Present frame 1
	swapchain.PresentDrawable(frame1);

	// Resize to 1280x720
	SDL_SetWindowSize(window, 1280, 720);
	swapchain.Resize(1280, 720);

	Check(swapchain.GetDrawableWidth() >= 1280, "Drawable width after resize must be >= 1280");
	Check(swapchain.GetDrawableHeight() >= 720, "Drawable height after resize must be >= 720");

	// Recreated acquisition after resize
	auto frame2 = swapchain.AcquireDrawable();
	Check(frame2.drawable != nullptr, "AcquireDrawable failed after resize");
	Check(frame2.width >= 1280, "Frame 2 width must match resized size");
	Check(frame2.height >= 720, "Frame 2 height must match resized size");

	swapchain.PresentDrawable(frame2);

	swapchain.Detach();
	SDL_DestroyWindow(window);
	backend.Shutdown();
	std::printf("  [OK] Phase D: Window Resize & Drawable Recreation\n");
#else
	std::printf("  [SKIP] Phase D: TestPhaseD_ResizeAndDrawableRecreation (non-Apple)\n");
#endif
}

void TestPhaseD_FullscreenToggle() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");

	SDL_Window* window = SDL_CreateWindow("TestPhaseD_Fullscreen",
	                                      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                                      1024, 768, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	Check(window != nullptr, "SDL_CreateWindow failed");

	Libs::Graphics::MetalSwapchain swapchain;
	Check(swapchain.Attach(window, backend.GetMTLDevice()), "Attach failed");

	// Toggle fake/desktop fullscreen flag
	SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);

	int fw = 0, fh = 0;
	SDL_GetWindowSize(window, &fw, &fh);
	swapchain.Resize(static_cast<uint32_t>(fw), static_cast<uint32_t>(fh));

	auto frame = swapchain.AcquireDrawable();
	Check(frame.drawable != nullptr, "AcquireDrawable in fullscreen failed");
	swapchain.PresentDrawable(frame);

	// Restore windowed mode
	SDL_SetWindowFullscreen(window, 0);
	swapchain.Resize(1024, 768);

	swapchain.Detach();
	SDL_DestroyWindow(window);
	backend.Shutdown();
	std::printf("  [OK] Phase D: Fullscreen Toggle & Presentation\n");
#else
	std::printf("  [SKIP] Phase D: TestPhaseD_FullscreenToggle (non-Apple)\n");
#endif
}

void TestPhaseD_MultipleWindows() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");

	SDL_Window* win1 = SDL_CreateWindow("Win1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	SDL_Window* win2 = SDL_CreateWindow("Win2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	Check(win1 != nullptr && win2 != nullptr, "Multiple SDL window creation failed");

	Libs::Graphics::MetalSwapchain swap1;
	Libs::Graphics::MetalSwapchain swap2;

	Check(swap1.Attach(win1, backend.GetMTLDevice()), "Attach win1 failed");
	Check(swap2.Attach(win2, backend.GetMTLDevice()), "Attach win2 failed");

	auto f1 = swap1.AcquireDrawable();
	auto f2 = swap2.AcquireDrawable();

	Check(f1.drawable != nullptr, "Win1 acquire failed");
	Check(f2.drawable != nullptr, "Win2 acquire failed");

	swap1.PresentDrawable(f1);
	swap2.PresentDrawable(f2);

	swap1.Detach();
	swap2.Detach();

	SDL_DestroyWindow(win1);
	SDL_DestroyWindow(win2);
	backend.Shutdown();
	std::printf("  [OK] Phase D: Multiple Independent Metal Windows\n");
#else
	std::printf("  [SKIP] Phase D: TestPhaseD_MultipleWindows (non-Apple)\n");
#endif
}

void BenchmarkPhaseD_DrawableAcquisitionLatency() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed for benchmark");

	SDL_Window* window = SDL_CreateWindow("BenchWindow", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_HIDDEN | SDL_WINDOW_ALLOW_HIGHDPI);
	Check(window != nullptr, "SDL_CreateWindow failed");

	Libs::Graphics::MetalSwapchain swapchain;
	Check(swapchain.Attach(window, backend.GetMTLDevice()), "Attach failed");

	static constexpr size_t FRAMES = 100;
	const auto t0 = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < FRAMES; ++i) {
		auto frame = swapchain.AcquireDrawable();
		Check(frame.drawable != nullptr, "Acquire failed during bench");
		swapchain.PresentDrawable(frame);
	}
	const auto t1 = std::chrono::high_resolution_clock::now();

	double total_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	double avg_acq_us = swapchain.GetAverageAcquireLatencyNs() / 1000.0;

	std::printf("  [Bench] CAMetalLayer Drawable Acquisition Latency: %.2f µs/frame (Total: %.2f ms over %zu frames)\n",
	            avg_acq_us, total_ms, FRAMES);

	swapchain.Detach();
	SDL_DestroyWindow(window);
	backend.Shutdown();
#else
	std::printf("  [SKIP] BenchmarkPhaseD_DrawableAcquisitionLatency (non-Apple)\n");
#endif
}

// ─── Phase E: Metal Pipeline Cache Tests & Benchmarks ──────────────────────────

void TestPhaseE_GraphicsPipelineCacheReuse() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init for Phase E failed");

	Libs::Graphics::MetalPipelineCache* cache = backend.GetPipelineCache();
	Check(cache != nullptr, "GetPipelineCache() must not be null");

	Libs::Graphics::MetalGraphicsPipelineKey key {};
	key.vs_shader_id = Libs::Graphics::ShaderId{100, 200, {1, 2}};
	key.ps_shader_id = Libs::Graphics::ShaderId{300, 400, {3, 4}};
	key.rendering.color_count = 1;
	key.rendering.color_formats[0] = vk::Format::eB8G8R8A8Unorm;

	// First creation — Cache Miss & Compile
	auto* pipeline1 = cache->GetOrCreateGraphicsPipeline(key);
	Check(pipeline1 != nullptr, "Graphics pipeline compilation failed");
	Check(pipeline1->render_pipeline_state != nullptr, "MTLRenderPipelineState must not be null");
	Check(cache->GetGraphicsHits() == 0, "Initial hit count must be 0");
	Check(cache->GetGraphicsMisses() == 1, "Initial miss count must be 1");
	Check(cache->GetGraphicsCacheSize() == 1, "Cache size must be 1");

	// Second lookup — Cache Hit & Reuse
	auto* pipeline2 = cache->GetOrCreateGraphicsPipeline(key);
	Check(pipeline2 != nullptr, "Graphics pipeline lookup failed");
	Check(pipeline1 == pipeline2, "Must return exact same cached pipeline pointer (no duplicate compilation)");
	Check(cache->GetGraphicsHits() == 1, "Hit count must be 1 after reuse");
	Check(cache->GetGraphicsMisses() == 1, "Miss count must stay 1");
	Check(cache->GetGraphicsHitRate() == 50.0, "Hit rate must be 50.0%");

	backend.Shutdown();
	std::printf("  [OK] Phase E: Graphics Pipeline Cache Reuse & Deduplication\n");
#else
	std::printf("  [SKIP] Phase E: TestPhaseE_GraphicsPipelineCacheReuse (non-Apple)\n");
#endif
}

void TestPhaseE_ComputePipelineCacheReuse() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Metal backend init failed");

	Libs::Graphics::MetalPipelineCache* cache = backend.GetPipelineCache();
	Check(cache != nullptr, "GetPipelineCache() must not be null");

	Libs::Graphics::MetalComputePipelineKey key {};
	key.cs_shader_id = Libs::Graphics::ShaderId{500, 600, {5, 6}};

	auto* pipe1 = cache->GetOrCreateComputePipeline(key);
	Check(pipe1 != nullptr, "Compute pipeline compilation failed");
	Check(pipe1->compute_pipeline_state != nullptr, "MTLComputePipelineState must not be null");
	Check(cache->GetComputeMisses() == 1, "Miss count must be 1");

	auto* pipe2 = cache->GetOrCreateComputePipeline(key);
	Check(pipe2 == pipe1, "Compute pipeline must be reused without duplicate compilation");
	Check(cache->GetComputeHits() == 1, "Compute hit count must be 1");

	backend.Shutdown();
	std::printf("  [OK] Phase E: Compute Pipeline Cache Reuse & Deduplication\n");
#else
	std::printf("  [SKIP] Phase E: TestPhaseE_ComputePipelineCacheReuse (non-Apple)\n");
#endif
}

void TestPhaseE_LRUEviction() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed");

	Libs::Graphics::MetalPipelineCache cache(backend.GetMTLDevice(), 3, 3);

	for (uint32_t i = 1; i <= 4; ++i) {
		Libs::Graphics::MetalComputePipelineKey key {};
		key.cs_shader_id = Libs::Graphics::ShaderId{i, i * 10, {i}};
		auto* p = cache.GetOrCreateComputePipeline(key);
		Check(p != nullptr, "Compute pipeline creation failed");
	}

	Check(cache.GetComputeCacheSize() <= 3, "Compute cache size must be bounded by capacity limit (<= 3)");
	Check(cache.GetComputeMisses() == 4, "Must have 4 total misses");

	std::printf("  [OK] Phase E: LRU Eviction Bounded Capacity Enforcement\n");
	backend.Shutdown();
#else
	std::printf("  [SKIP] Phase E: TestPhaseE_LRUEviction (non-Apple)\n");
#endif
}

void BenchmarkPhaseE_PipelineCreationAndLookup() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for benchmark");

	Libs::Graphics::MetalPipelineCache* cache = backend.GetPipelineCache();

	static constexpr size_t NUM_PIPELINES = 50;
	static constexpr size_t LOOKUP_ITERS = 10000;

	std::vector<Libs::Graphics::MetalGraphicsPipelineKey> keys;
	keys.reserve(NUM_PIPELINES);

	for (size_t i = 0; i < NUM_PIPELINES; ++i) {
		Libs::Graphics::MetalGraphicsPipelineKey key {};
		key.vs_shader_id = Libs::Graphics::ShaderId{static_cast<uint32_t>(i + 1), static_cast<uint32_t>(i * 10), {static_cast<uint32_t>(i)}};
		key.ps_shader_id = Libs::Graphics::ShaderId{static_cast<uint32_t>(i + 100), static_cast<uint32_t>(i * 20), {static_cast<uint32_t>(i)}};
		key.rendering.color_count = 1;
		key.rendering.color_formats[0] = vk::Format::eB8G8R8A8Unorm;
		keys.push_back(key);
	}

	const auto t_create_start = std::chrono::high_resolution_clock::now();
	for (const auto& key : keys) {
		auto* p = cache->GetOrCreateGraphicsPipeline(key);
		Check(p != nullptr, "Pipeline creation failed during bench");
	}
	const auto t_create_end = std::chrono::high_resolution_clock::now();

	double create_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t_create_end - t_create_start).count()) / 1000.0;
	double avg_create_ms = create_ms / static_cast<double>(NUM_PIPELINES);

	const auto t_lookup_start = std::chrono::high_resolution_clock::now();
	for (size_t iter = 0; iter < LOOKUP_ITERS; ++iter) {
		const auto& key = keys[iter % NUM_PIPELINES];
		auto* p = cache->GetOrCreateGraphicsPipeline(key);
		Check(p != nullptr, "Lookup failed");
	}
	const auto t_lookup_end = std::chrono::high_resolution_clock::now();

	double lookup_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t_lookup_end - t_lookup_start).count()) / 1000.0;
	double lookup_ns_per_op = (lookup_ms * 1e6) / static_cast<double>(LOOKUP_ITERS);

	std::printf("  [Bench] Metal Pipeline Creation Latency: %.3f ms/pipeline (%zu unique pipelines)\n", avg_create_ms, NUM_PIPELINES);
	std::printf("  [Bench] Pipeline Cache Lookup Latency: %.2f ns/lookup (Total: %.2f ms over %zu lookups)\n", lookup_ns_per_op, lookup_ms, LOOKUP_ITERS);
	std::printf("  [Bench] Cache Hit Rate: %.2f%% (Hits: %llu, Misses: %llu)\n", cache->GetGraphicsHitRate(), static_cast<unsigned long long>(cache->GetGraphicsHits()), static_cast<unsigned long long>(cache->GetGraphicsMisses()));
	std::printf("  [Bench] Estimated Cache Memory Usage: %zu KB (%zu bytes)\n", cache->GetTotalEstimatedMemoryUsageBytes() / 1024, cache->GetTotalEstimatedMemoryUsageBytes());

	backend.Shutdown();
#else
	std::printf("  [SKIP] BenchmarkPhaseE_PipelineCreationAndLookup (non-Apple)\n");
#endif
}

// ─── Phase F: Metal Resource Binding & Argument Buffers ─────────────────────

void TestPhaseF_DescriptorTranslationAndResourceUpdates() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for Phase F");

	Libs::Graphics::MetalArgumentBufferCache* arg_cache = backend.GetArgumentBufferCache();
	Check(arg_cache != nullptr, "GetArgumentBufferCache() must not be null");

	// Create mock Vulkan/engine NativeDescriptors
	Libs::Graphics::DescriptorCache::NativeDescriptors native;

	auto mock_owner1 = std::make_shared<int>(42);
	auto mock_owner2 = std::make_shared<double>(3.14);

	Libs::Graphics::BufferView bv1;
	bv1.owner  = mock_owner1;
	bv1.buffer = static_cast<vk::Buffer>(VkBuffer(0x1000));
	bv1.offset = 64;
	bv1.range  = 1024;

	Libs::Graphics::BufferView bv2;
	bv2.owner  = mock_owner2;
	bv2.buffer = static_cast<vk::Buffer>(VkBuffer(0x2000));
	bv2.offset = 128;
	bv2.range  = 2048;

	native.buffers.push_back(bv1);
	native.buffers.push_back(bv2);

	Libs::Graphics::DescriptorCache::TextureBinding tex1;
	tex1.image_view = static_cast<vk::ImageView>(VkImageView(0x3000));
	native.images.push_back(tex1);

	native.samplers.push_back(static_cast<vk::Sampler>(VkSampler(0x4000)));

	// Translate with dynamic offsets
	std::vector<uint32_t> dynamic_offsets = {128, 256};
	Libs::Graphics::MetalResourceSet res_set = arg_cache->TranslateNativeDescriptors(native, dynamic_offsets);

	Check(res_set.buffers.size() == 2, "Translated buffer count must be 2");
	Check(res_set.textures.size() == 1, "Translated texture count must be 1");
	Check(res_set.samplers.size() == 1, "Translated sampler count must be 1");
	Check(res_set.lifetime_owners.size() == 2, "Lifetime owners count must be 2 (mock_owner1 & 2)");

	// Verify dynamic offsets applied: 64 + 128 = 192, 128 + 256 = 384
	Check(res_set.buffers[0].offset == 192, "Buffer 0 offset must be base + dynamic offset (64 + 128 = 192)");
	Check(res_set.buffers[1].offset == 384, "Buffer 1 offset must be base + dynamic offset (128 + 256 = 384)");

	backend.Shutdown();
	std::printf("  [OK] Phase F: Descriptor Translation & Dynamic Resource Updates\n");
#else
	std::printf("  [SKIP] Phase F: TestPhaseF_DescriptorTranslationAndResourceUpdates (non-Apple)\n");
#endif
}

void TestPhaseF_DynamicOffsetsAndLifetime() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed");

	Libs::Graphics::MetalArgumentBuffer arg_buf(backend.GetMTLDevice(), 4096);
	Check(arg_buf.IsValid(), "MetalArgumentBuffer must be valid");

	Libs::Graphics::MetalResourceSet res_set;
	Libs::Graphics::MetalBufferBinding b;
	b.buffer = (void*)0x5000;
	b.offset = 100;
	b.range  = 512;
	b.slot   = 0;
	res_set.buffers.push_back(b);

	auto lifetime_token = std::make_shared<std::string>("GPU_IN_FLIGHT_RESOURCE");
	res_set.lifetime_owners.push_back(lifetime_token);

	Check(arg_buf.EncodeResourceSet(res_set), "EncodeResourceSet failed");
	Check(lifetime_token.use_count() >= 2, "Lifetime owner shared_ptr use_count must increase during buffer binding");

	// Update dynamic offset for slot 0
	Check(arg_buf.UpdateDynamicOffset(0, 1024), "UpdateDynamicOffset failed");
	Check(arg_buf.GetEncodedResourceSet().buffers[0].offset == 1024, "Offset must be updated to 1024");

	backend.Shutdown();
	std::printf("  [OK] Phase F: Dynamic Offsets & Resource Lifetime Retention\n");
#else
	std::printf("  [SKIP] Phase F: TestPhaseF_DynamicOffsetsAndLifetime (non-Apple)\n");
#endif
}

void TestPhaseF_MultipleDescriptorLayoutsAndArgumentBufferPooling() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed");

	Libs::Graphics::MetalArgumentBufferCache* arg_cache = backend.GetArgumentBufferCache();

	Libs::Graphics::MetalArgumentBufferLayout layout1 {2, 2, 1};
	Libs::Graphics::MetalArgumentBufferLayout layout2 {4, 4, 2};

	Libs::Graphics::MetalResourceSet set1;
	set1.buffers.push_back({(void*)0x1000, 0, 256, 0});
	set1.textures.push_back({(void*)0x2000, 0});

	// Request Layout 1 — Miss & Pool Creation
	auto* buf1 = arg_cache->GetOrCreateArgumentBuffer(layout1, set1);
	Check(buf1 != nullptr, "Argument buffer allocation failed");
	Check(arg_cache->GetMisses() == 1, "Miss count must be 1");

	// Request Layout 1 with same resource set — Hit & Pool Reuse
	auto* buf2 = arg_cache->GetOrCreateArgumentBuffer(layout1, set1);
	Check(buf2 == buf1, "Must reuse pooled argument buffer for identical layout & resources");
	Check(arg_cache->GetHits() == 1, "Hit count must be 1");

	// Request Layout 2 — Distinct layout miss
	auto* buf3 = arg_cache->GetOrCreateArgumentBuffer(layout2, set1);
	Check(buf3 != nullptr && buf3 != buf1, "Distinct layout must create a separate argument buffer");
	Check(arg_cache->GetMisses() == 2, "Miss count must be 2");

	backend.Shutdown();
	std::printf("  [OK] Phase F: Multiple Descriptor Layouts & Argument Buffer Pooling\n");
#else
	std::printf("  [SKIP] Phase F: TestPhaseF_MultipleDescriptorLayoutsAndArgumentBufferPooling (non-Apple)\n");
#endif
}

void BenchmarkPhaseF_DescriptorBindingPerformance() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for bench");

	Libs::Graphics::MetalArgumentBufferCache* arg_cache = backend.GetArgumentBufferCache();

	Libs::Graphics::DescriptorCache::NativeDescriptors native;
	for (size_t i = 0; i < 4; ++i) {
		Libs::Graphics::BufferView bv;
		bv.buffer = static_cast<vk::Buffer>(VkBuffer(0x1000 + i * 0x100));
		bv.offset = i * 64;
		bv.range  = 512;
		native.buffers.push_back(bv);
	}
	for (size_t i = 0; i < 4; ++i) {
		Libs::Graphics::DescriptorCache::TextureBinding tex;
		tex.image_view = static_cast<vk::ImageView>(VkImageView(0x5000 + i * 0x100));
		native.images.push_back(tex);
	}
	native.samplers.push_back(static_cast<vk::Sampler>(VkSampler(0x9000)));

	Libs::Graphics::MetalArgumentBufferLayout layout {4, 4, 1};
	static constexpr size_t ITERS = 10000;

	const auto t0 = std::chrono::high_resolution_clock::now();
	for (size_t iter = 0; iter < ITERS; ++iter) {
		std::vector<uint32_t> dyn_offsets = {static_cast<uint32_t>(iter * 16), static_cast<uint32_t>(iter * 32)};
		auto set = arg_cache->TranslateNativeDescriptors(native, dyn_offsets);
		auto* arg_buf = arg_cache->GetOrCreateArgumentBuffer(layout, set);
		Check(arg_buf != nullptr, "Argument buffer lookup failed in bench");
	}
	const auto t1 = std::chrono::high_resolution_clock::now();

	double total_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()) / 1000.0;
	double binding_ns_per_op = (total_ms * 1e6) / static_cast<double>(ITERS);

	std::printf("  [Bench] Metal Descriptor Translation & Binding Latency: %.2f ns/binding (Total: %.2f ms over %zu iterations)\n",
	            binding_ns_per_op, total_ms, ITERS);
	std::printf("  [Bench] Argument Buffer Pool Hit Rate: %.2f%% (Hits: %llu, Misses: %llu)\n",
	            arg_cache->GetHitRate(), static_cast<unsigned long long>(arg_cache->GetHits()), static_cast<unsigned long long>(arg_cache->GetMisses()));

	backend.Shutdown();
#else
	std::printf("  [SKIP] BenchmarkPhaseF_DescriptorBindingPerformance (non-Apple)\n");
#endif
}

// ─── Phase H: Metal Synchronization (Fences, Events, Hazards & Frame Pacing) ───

void TestPhaseH_MTLFenceIntraQueueSync() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for Phase H Fence test");

	Libs::Graphics::HostGpu::Metal::MetalFence fence;
	Check(fence.Initialize(backend.GetMTLDevice()), "MetalFence init failed");
	Check(fence.IsValid(), "MetalFence must be valid");

	Libs::Graphics::MetalCommandBuffer cmd_buf(backend.GetMTLCommandQueue());
	Check(cmd_buf.OpenComputeEncoder() != nullptr, "OpenComputeEncoder 1 failed");
	fence.UpdateInComputeEncoder(cmd_buf.GetNativeComputeEncoder());
	cmd_buf.CloseComputeEncoder();

	Check(cmd_buf.OpenComputeEncoder() != nullptr, "OpenComputeEncoder 2 failed");
	fence.WaitForInComputeEncoder(cmd_buf.GetNativeComputeEncoder());
	cmd_buf.CloseComputeEncoder();

	cmd_buf.Commit();
	cmd_buf.WaitUntilCompleted();

	fence.Reset();
	Check(!fence.IsValid(), "MetalFence must be invalid after Reset()");

	backend.Shutdown();
	std::printf("  [OK] Phase H: MTLFence Intra-Queue Synchronization & Encoder Barriers\n");
#else
	std::printf("  [SKIP] Phase H: TestPhaseH_MTLFenceIntraQueueSync (non-Apple)\n");
#endif
}

void TestPhaseH_MTLEventInterQueueSync() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for Phase H Event test");

	Libs::Graphics::HostGpu::Metal::MetalEvent event;
	Check(event.Initialize(backend.GetMTLDevice(), true), "MetalSharedEvent init failed");
	Check(event.IsValid(), "MetalEvent must be valid");
	Check(event.IsShared(), "MetalEvent must be shared");

	Libs::Graphics::MetalCommandBuffer buf_a(backend.GetMTLCommandQueue());
	event.SignalOnCommandBuffer(buf_a.GetNativeCommandBuffer(), 42);
	buf_a.Commit();
	buf_a.WaitUntilCompleted();

	Check(event.GetSignaledValue() == 42, "Event signaled value must be 42");

	event.SignalFromHost(100);
	Check(event.GetSignaledValue() == 100, "Event signaled value after host signal must be 100");

	event.Reset();
	backend.Shutdown();
	std::printf("  [OK] Phase H: MTLEvent / MTLSharedEvent Inter-Queue Synchronization\n");
#else
	std::printf("  [SKIP] Phase H: TestPhaseH_MTLEventInterQueueSync (non-Apple)\n");
#endif
}

void TestPhaseH_ResourceHazardTracking() {
	Libs::Graphics::HostGpu::Metal::MetalResourceHazardTracker tracker;

	bool raw = false, war = false, waw = false;

	// Initial Write — No hazards
	tracker.TrackResourceAccess(1001, Libs::Graphics::HostGpu::Metal::MetalResourceAccess::Write, raw, war, waw);
	Check(!raw && !war && !waw, "Initial Write must produce no hazards");

	// Read after Write — RAW hazard
	tracker.TrackResourceAccess(1001, Libs::Graphics::HostGpu::Metal::MetalResourceAccess::Read, raw, war, waw);
	Check(raw && !war && !waw, "Read after Write must produce RAW hazard");

	// Write after Read — WAR hazard
	tracker.TrackResourceAccess(1001, Libs::Graphics::HostGpu::Metal::MetalResourceAccess::Write, raw, war, waw);
	Check(!raw && war && !waw, "Write after Read must produce WAR hazard");

	// Write after Write — WAW hazard
	tracker.TrackResourceAccess(1001, Libs::Graphics::HostGpu::Metal::MetalResourceAccess::Write, raw, war, waw);
	Check(!raw && !war && waw, "Write after Write must produce WAW hazard");

	tracker.Reset();
	std::printf("  [OK] Phase H: Resource Hazard Resolution (RAW / WAR / WAW)\n");
}

void TestPhaseH_FramePacingAndInFlightThrottling() {
	Libs::Graphics::HostGpu::Metal::MetalFrameSync frame_sync(2);
	Check(frame_sync.GetMaxFramesInFlight() == 2, "Max frames in flight must be 2");

	frame_sync.BeginFrame();
	frame_sync.EndFrame(nullptr);

	frame_sync.BeginFrame();
	frame_sync.EndFrame(nullptr);

	Check(frame_sync.GetTotalFramesPresented() == 2, "Total frames presented must be 2");
	std::printf("  [OK] Phase H: Frame Pacing & In-Flight Frame Semaphore Throttling\n");
}

void TestPhaseH_MultiQueueParallelEncodingStress() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for multi-queue stress test");

	static constexpr size_t NUM_THREADS = 4;
	static constexpr size_t ENCODES_PER_THREAD = 10;

	std::vector<std::thread> workers;
	workers.reserve(NUM_THREADS);

	for (size_t t = 0; t < NUM_THREADS; ++t) {
		workers.emplace_back([&backend]() {
			for (size_t i = 0; i < ENCODES_PER_THREAD; ++i) {
				Libs::Graphics::MetalCommandBuffer buf(backend.GetMTLCommandQueue());
				if (buf.OpenComputeEncoder() != nullptr) {
					buf.CloseComputeEncoder();
				}
				buf.Commit();
				buf.WaitUntilCompleted();
			}
		});
	}

	for (auto& w : workers) {
		w.join();
	}

	backend.WaitIdle();
	backend.Shutdown();
	std::printf("  [OK] Phase H: Multi-Queue Parallel Command Encoding Stress Test (%zu threads x %zu passes)\n", NUM_THREADS, ENCODES_PER_THREAD);
#else
	std::printf("  [SKIP] Phase H: TestPhaseH_MultiQueueParallelEncodingStress (non-Apple)\n");
#endif
}

void BenchmarkPhaseH_SynchronizationOverhead() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend backend;
	Check(backend.Initialize(), "Backend init failed for sync benchmark");

	static constexpr size_t ITERS = 1000;

	// 1. MTLFence Latency
	Libs::Graphics::HostGpu::Metal::MetalFence fence;
	Check(fence.Initialize(backend.GetMTLDevice()), "Fence init failed");

	const auto t_fence_start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERS; ++i) {
		Libs::Graphics::MetalCommandBuffer buf(backend.GetMTLCommandQueue());
		if (buf.OpenComputeEncoder() != nullptr) {
			fence.UpdateInComputeEncoder(buf.GetNativeComputeEncoder());
			fence.WaitForInComputeEncoder(buf.GetNativeComputeEncoder());
			buf.CloseComputeEncoder();
		}
		buf.Commit();
	}
	const auto t_fence_end = std::chrono::high_resolution_clock::now();

	double fence_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t_fence_end - t_fence_start).count()) / 1000.0;
	double fence_us_per_op = (fence_ms * 1000.0) / static_cast<double>(ITERS);

	// 2. MTLEvent Latency
	Libs::Graphics::HostGpu::Metal::MetalEvent event;
	Check(event.Initialize(backend.GetMTLDevice(), true), "Event init failed");

	const auto t_event_start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERS; ++i) {
		Libs::Graphics::MetalCommandBuffer buf(backend.GetMTLCommandQueue());
		event.SignalOnCommandBuffer(buf.GetNativeCommandBuffer(), i + 1);
		buf.Commit();
	}
	const auto t_event_end = std::chrono::high_resolution_clock::now();

	double event_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t_event_end - t_event_start).count()) / 1000.0;
	double event_us_per_op = (event_ms * 1000.0) / static_cast<double>(ITERS);

	// 3. Frame Sync Semaphore Latency
	Libs::Graphics::HostGpu::Metal::MetalFrameSync frame_sync(3);
	const auto t_frame_start = std::chrono::high_resolution_clock::now();
	for (size_t i = 0; i < ITERS; ++i) {
		frame_sync.BeginFrame();
		frame_sync.EndFrame(nullptr);
	}
	const auto t_frame_end = std::chrono::high_resolution_clock::now();

	double frame_ms = static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(t_frame_end - t_frame_start).count()) / 1000.0;
	double frame_ns_per_op = (frame_ms * 1e6) / static_cast<double>(ITERS);

	std::printf("  [Bench] MTLFence Encoder Synchronization Latency: %.2f µs/barrier (Total: %.2f ms over %zu iterations)\n", fence_us_per_op, fence_ms, ITERS);
	std::printf("  [Bench] MTLEvent Signal/Wait Latency: %.2f µs/event (Total: %.2f ms over %zu iterations)\n", event_us_per_op, event_ms, ITERS);
	std::printf("  [Bench] Frame Pacing Semaphore Acquisition Latency: %.2f ns/frame (Total: %.2f ms over %zu iterations)\n", frame_ns_per_op, frame_ms, ITERS);

	fence.Reset();
	event.Reset();
	backend.Shutdown();
#else
	std::printf("  [SKIP] BenchmarkPhaseH_SynchronizationOverhead (non-Apple)\n");
#endif
}

} // namespace

int main() {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		std::fprintf(stderr, "GraphicBackendTests: SDL_Init failed: %s\n", SDL_GetError());
		return 1;
	}

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

	std::printf("\n--- Phase D: CAMetalLayer Integration & Presentation ---\n\n");

	// Phase D
	TestPhaseD_AttachDetach();
	TestPhaseD_ResizeAndDrawableRecreation();
	TestPhaseD_FullscreenToggle();
	TestPhaseD_MultipleWindows();

	std::printf("\n");
	BenchmarkPhaseD_DrawableAcquisitionLatency();

	std::printf("\n--- Phase E: Metal Pipeline Cache & Compilation ---\n\n");

	// Phase E
	TestPhaseE_GraphicsPipelineCacheReuse();
	TestPhaseE_ComputePipelineCacheReuse();
	TestPhaseE_LRUEviction();

	std::printf("\n");
	BenchmarkPhaseE_PipelineCreationAndLookup();

	std::printf("\n--- Phase F: Metal Resource Binding & Argument Buffers ---\n\n");

	// Phase F
	TestPhaseF_DescriptorTranslationAndResourceUpdates();
	TestPhaseF_DynamicOffsetsAndLifetime();
	TestPhaseF_MultipleDescriptorLayoutsAndArgumentBufferPooling();

	std::printf("\n");
	BenchmarkPhaseF_DescriptorBindingPerformance();

	std::printf("\n--- Phase H: Metal Synchronization (Fences, Events, Hazards & Frame Pacing) ---\n\n");

	// Phase H
	TestPhaseH_MTLFenceIntraQueueSync();
	TestPhaseH_MTLEventInterQueueSync();
	TestPhaseH_ResourceHazardTracking();
	TestPhaseH_FramePacingAndInFlightThrottling();
	TestPhaseH_MultiQueueParallelEncodingStress();

	std::printf("\n");
	BenchmarkPhaseH_SynchronizationOverhead();

	std::printf("\nGraphicBackendTests: PASSED\n");

	SDL_Quit();
	return 0;
}
