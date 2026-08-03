// GraphicBackendTests.cpp
//
// Unit tests verifying Phase A & B Graphic Backend Interface, Factory runtime selection,
// and Native Metal Device Initialization & Capability Detection.

#include "graphics/host_gpu/renderer/backend/graphicBackend.h"
#include "graphics/host_gpu/renderer/backend/graphicBackendFactory.h"
#include "graphics/host_gpu/renderer/backend/metalGraphicBackend.h"
#include "graphics/host_gpu/renderer/backend/vulkanGraphicBackend.h"

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
	// Test that shutdown backend or unsupported invocation safely returns false
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

} // namespace

int main() {
	std::printf("====================================================\n");
	std::printf(" KytyPS5 Graphic Backend Interface Tests            \n");
	std::printf("====================================================\n\n");

	TestDefaultBackend();
	TestVulkanBackendCreation();
	TestMetalBackendCreation();
	TestMetalDeviceInitAndCapabilities();
	TestUnsupportedFallback();
	TestInitializationBenchmarking();

	std::printf("GraphicBackendTests: PASSED\n");
	return 0;
}
