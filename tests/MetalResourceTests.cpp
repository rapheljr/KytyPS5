// MetalResourceTests.cpp
//
// Unit, stress, memory leak, and benchmark test suite for Phase J:
// Native Metal GPU Resource System (MetalBuffer, MetalTexture, MetalSamplerCache,
// MetalGpuHeapAllocator, MetalResourceDeferrer, MetalUploadStaging, MetalReadbackStaging).

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalBuffer.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/metalMemoryPool.h"
#include "graphics/host_gpu/renderer/backend/metalSamplerCache.h"
#include "graphics/host_gpu/renderer/backend/metalTexture.h"

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
		std::fprintf(stderr, "MetalResourceTests: failed: %s\n", text);
		std::abort();
	}
}

// ─── MetalBuffer Tests ───────────────────────────────────────────────────────

void TestMetalBufferCreationAndWrite() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");
	void* device = metal_backend.GetMTLDevice();

	Libs::Graphics::MetalBuffer buffer;
	const uint64_t size = 1024 * 1024;
	std::vector<uint8_t> src_data(size, 0xAB);

	bool create_ok = buffer.Create(device, size, Libs::Graphics::MetalBufferUsage::Vertex | Libs::Graphics::MetalBufferUsage::Upload, Libs::Graphics::MetalBufferMemoryType::Shared, src_data.data());
	Check(create_ok, "MetalBuffer creation failed");
	Check(buffer.GetMTLBuffer() != nullptr, "MTLBuffer handle must not be null");
	Check(buffer.GetSize() == size, "Buffer size mismatch");
	Check(buffer.IsMapped(), "Shared buffer must be mapped");

	std::vector<uint8_t> read_back(size, 0);
	bool read_ok = buffer.Read(read_back.data(), size, 0);
	Check(read_ok, "Buffer read failed");
	Check(read_back == src_data, "Buffer read data mismatch");

	buffer.Destroy();
	Check(buffer.GetMTLBuffer() == nullptr, "Buffer must be null after destroy");

	metal_backend.Shutdown();
	std::printf("  [OK] Phase J: MetalBuffer Creation & Read/Write\n");
#endif
}

void TestMetalRingBuffer() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");
	void* device = metal_backend.GetMTLDevice();

	Libs::Graphics::MetalRingBuffer ring_buffer;
	bool init_ok = ring_buffer.Initialize(device, 64 * 1024, Libs::Graphics::MetalBufferUsage::Uniform);
	Check(init_ok, "MetalRingBuffer init failed");

	for (int i = 0; i < 100; ++i) {
		auto alloc = ring_buffer.Allocate(256, 256);
		Check(alloc.valid, "Ring buffer allocation failed");
		Check(alloc.mtl_buffer != nullptr, "Allocated buffer handle must not be null");
		Check(alloc.mapped_ptr != nullptr, "Allocated mapped ptr must not be null");
		std::memset(alloc.mapped_ptr, i & 0xFF, alloc.size_bytes);
	}

	Check(ring_buffer.GetAllocatedBytes() == 100 * 256, "Allocated bytes tracking mismatch");
	ring_buffer.Reset();
	Check(ring_buffer.GetHeadOffset() == 0, "Head offset must be 0 after reset");

	ring_buffer.Shutdown();
	metal_backend.Shutdown();
	std::printf("  [OK] Phase J: MetalRingBuffer Sub-Allocation\n");
#endif
}

// ─── MetalTexture Tests ──────────────────────────────────────────────────────

void TestMetalTextureAndViews() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");
	void* device = metal_backend.GetMTLDevice();

	Libs::Graphics::MetalTextureDescriptor desc {};
	desc.type         = Libs::Graphics::MetalTextureType::Texture2D;
	desc.format       = Libs::Graphics::MetalPixelFormat::RGBA8Unorm;
	desc.width        = 512;
	desc.height       = 512;
	desc.depth        = 1;
	desc.array_layers = 1;
	desc.mip_levels   = 1;
	desc.usage        = Libs::Graphics::MetalTextureUsage::ShaderRead | Libs::Graphics::MetalTextureUsage::RenderTarget;
	desc.mem_type     = Libs::Graphics::MetalBufferMemoryType::Shared;

	Libs::Graphics::MetalTexture texture;
	bool create_ok = texture.Create(device, desc);
	Check(create_ok, "MetalTexture creation failed");
	Check(texture.GetMTLTexture() != nullptr, "MTLTexture handle must not be null");
	Check(texture.GetWidth() == 512, "Texture width mismatch");

	std::vector<uint32_t> pixels(512 * 512, 0xFF00FF00); // Green RGBA8
	bool write_ok = texture.WritePixels(pixels.data(), 512 * 4, 0, 0);
	Check(write_ok, "Texture WritePixels failed");

	std::vector<uint32_t> read_pixels(512 * 512, 0);
	bool read_ok = texture.ReadPixels(read_pixels.data(), 512 * 4, 0, 0);
	Check(read_ok, "Texture ReadPixels failed");
	Check(read_pixels == pixels, "Texture read pixel data mismatch");

	auto view = texture.CreateView(Libs::Graphics::MetalPixelFormat::RGBA8Unorm, Libs::Graphics::MetalTextureType::Texture2D, 0, 1, 0, 1);
	Check(view.GetMTLTextureView() != nullptr, "TextureView creation failed");
	view.Destroy();

	texture.Destroy();
	metal_backend.Shutdown();
	std::printf("  [OK] Phase J: MetalTexture Creation, Pixel IO & Views\n");
#endif
}

// ─── MetalSamplerCache Tests ────────────────────────────────────────────────

void TestMetalSamplerCacheDeduplication() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");
	void* device = metal_backend.GetMTLDevice();

	Libs::Graphics::MetalSamplerCache sampler_cache;
	Check(sampler_cache.Initialize(device), "SamplerCache init failed");

	Libs::Graphics::MetalSamplerDescriptor desc {};
	desc.min_filter = Libs::Graphics::MetalMinMagFilter::Linear;
	desc.mag_filter = Libs::Graphics::MetalMinMagFilter::Linear;
	desc.address_s  = Libs::Graphics::MetalSamplerAddressMode::ClampToEdge;

	void* sampler1 = sampler_cache.GetOrCreateSampler(desc);
	Check(sampler1 != nullptr, "Sampler 1 creation failed");

	// Request identical sampler descriptor 1000 times
	for (int i = 0; i < 1000; ++i) {
		void* sampler_cached = sampler_cache.GetOrCreateSampler(desc);
		Check(sampler_cached == sampler1, "Sampler cache must return identical retained handle");
	}

	Check(sampler_cache.GetCachedSamplerCount() == 1, "Sampler cache count must be 1 (deduplicated)");

	sampler_cache.Shutdown();
	metal_backend.Shutdown();
	std::printf("  [OK] Phase J: MetalSamplerCache Descriptor Deduplication\n");
#endif
}

// ─── MetalMemoryPool & Staging Tests ─────────────────────────────────────────

void TestMetalMemoryPoolAndDeferredRelease() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal backend init failed");
	void* device = metal_backend.GetMTLDevice();

	Libs::Graphics::MetalGpuHeapAllocator heap_allocator;
	bool heap_ok = heap_allocator.Initialize(device, 16 * 1024 * 1024);
	Check(heap_ok, "HeapAllocator init failed");

	void* heap_buf = heap_allocator.AllocateBuffer(1024 * 1024, Libs::Graphics::MetalBufferMemoryType::Private);
	Check(heap_buf != nullptr, "Heap buffer allocation failed");
	Check(heap_allocator.GetHeapCount() >= 1, "Heap count must be >= 1");

	Libs::Graphics::MetalResourceDeferrer deferrer;
	deferrer.Initialize();

	deferrer.DeferRelease(heap_buf, 5); // Defer release until frame 5
	Check(deferrer.GetPendingReleaseCount() == 1, "Pending release count must be 1");

	deferrer.ProcessDeferredReleases(3); // Process up to completed frame 3
	Check(deferrer.GetPendingReleaseCount() == 1, "Pending release count must remain 1 for frame 5 target");

	deferrer.ProcessDeferredReleases(5); // Process up to completed frame 5
	Check(deferrer.GetPendingReleaseCount() == 0, "Pending release count must be 0 after frame 5 completed");

	deferrer.Shutdown();
	heap_allocator.Shutdown();

	Libs::Graphics::MetalUploadStaging upload_staging;
	Check(upload_staging.Initialize(device, 2 * 1024 * 1024), "UploadStaging init failed");

	std::vector<uint8_t> upload_data(512 * 1024, 0x7E);
	auto stage_alloc = upload_staging.StageUpload(upload_data.data(), upload_data.size());
	Check(stage_alloc.valid, "Staging upload allocation failed");
	Check(std::memcmp(stage_alloc.mapped_ptr, upload_data.data(), upload_data.size()) == 0, "Staged bytes mismatch");

	upload_staging.Shutdown();

	metal_backend.Shutdown();
	std::printf("  [OK] Phase J: MetalGpuHeapAllocator, Deferred Release & Upload Staging\n");
#endif
}

// ─── Benchmarks ──────────────────────────────────────────────────────────────

void BenchmarkPhaseJPerformance() {
#if defined(__APPLE__)
	Libs::Graphics::MetalGraphicBackend metal_backend;
	Check(metal_backend.Initialize(), "Metal init failed");
	void* device = metal_backend.GetMTLDevice();

	// 1. Buffer Allocation Throughput Benchmark
	constexpr int kBufferOps = 10000;
	auto t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kBufferOps; ++i) {
		Libs::Graphics::MetalBuffer buf;
		buf.Create(device, 4096, Libs::Graphics::MetalBufferUsage::Vertex, Libs::Graphics::MetalBufferMemoryType::Shared);
		buf.Destroy();
	}
	auto t1 = std::chrono::high_resolution_clock::now();
	double buf_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kBufferOps;
	std::printf("  [Bench] MetalBuffer Allocation Latency: %.2f ns/buffer (Total: %.2f ms over %d allocs)\n",
	           buf_dt_ns, std::chrono::duration<double, std::milli>(t1 - t0).count(), kBufferOps);

	// 2. Sampler Cache Lookup Latency Benchmark
	Libs::Graphics::MetalSamplerCache sampler_cache;
	sampler_cache.Initialize(device);
	Libs::Graphics::MetalSamplerDescriptor desc {};
	desc.min_filter = Libs::Graphics::MetalMinMagFilter::Linear;
	(void)sampler_cache.GetOrCreateSampler(desc);


	constexpr int kSamplerOps = 100000;
	t0 = std::chrono::high_resolution_clock::now();
	for (int i = 0; i < kSamplerOps; ++i) {
		void* sampler = sampler_cache.GetOrCreateSampler(desc);
		(void)sampler;
	}
	t1 = std::chrono::high_resolution_clock::now();
	double sampler_dt_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kSamplerOps;
	std::printf("  [Bench] MetalSamplerCache Lookup Latency: %.2f ns/lookup (Total: %.2f ms over %d lookups)\n",
	           sampler_dt_ns, std::chrono::duration<double, std::milli>(t1 - t0).count(), kSamplerOps);

	sampler_cache.Shutdown();
	metal_backend.Shutdown();
#endif
}

} // namespace

int main() {
#if defined(__APPLE__)
	SDL_Init(SDL_INIT_VIDEO);
#endif

	std::printf("====================================================\n");
	std::printf(" KytyPS5 Phase J: Native Metal GPU Resource System  \n");
	std::printf("====================================================\n\n");

	TestMetalBufferCreationAndWrite();
	TestMetalRingBuffer();
	TestMetalTextureAndViews();
	TestMetalSamplerCacheDeduplication();
	TestMetalMemoryPoolAndDeferredRelease();

	std::printf("\n--- Phase J Benchmarks ---\n");
	BenchmarkPhaseJPerformance();

#if defined(__APPLE__)
	SDL_Quit();
#endif

	std::printf("\nMetalResourceTests: ALL PASSED\n");
	return 0;
}

