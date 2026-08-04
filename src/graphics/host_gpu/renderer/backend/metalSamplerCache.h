#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSAMPLERCACHE_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSAMPLERCACHE_H_

#include "common/common.h"

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace Libs::Graphics {

enum class MetalMinMagFilter : uint8_t {
	Nearest,
	Linear
};

enum class MetalMipFilter : uint8_t {
	NotMipmapped,
	Nearest,
	Linear
};

enum class MetalSamplerAddressMode : uint8_t {
	ClampToEdge,
	Repeat,
	MirrorRepeat,
	ClampToZero
};

enum class MetalCompareFunction : uint8_t {
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	NotEqual,
	GreaterEqual,
	Always
};

struct MetalSamplerDescriptor {
	MetalMinMagFilter          min_filter        = MetalMinMagFilter::Linear;
	MetalMinMagFilter          mag_filter        = MetalMinMagFilter::Linear;
	MetalMipFilter             mip_filter        = MetalMipFilter::Linear;
	MetalSamplerAddressMode    address_s         = MetalSamplerAddressMode::ClampToEdge;
	MetalSamplerAddressMode    address_t         = MetalSamplerAddressMode::ClampToEdge;
	MetalSamplerAddressMode    address_r         = MetalSamplerAddressMode::ClampToEdge;
	uint32_t                   max_anisotropy    = 1;
	float                      lod_min           = 0.0f;
	float                      lod_max           = 1000.0f;
	MetalCompareFunction       compare_func      = MetalCompareFunction::Never;
	bool                       normalized_coords = true;

	[[nodiscard]] uint64_t ComputeHash() const noexcept;
	bool operator==(const MetalSamplerDescriptor& other) const noexcept;
};

class MetalSamplerCache {
public:
	MetalSamplerCache() = default;
	~MetalSamplerCache();

	MetalSamplerCache(const MetalSamplerCache&) = delete;
	MetalSamplerCache& operator=(const MetalSamplerCache&) = delete;

	bool Initialize(void* mtl_device);
	void Shutdown();

	[[nodiscard]] void* GetOrCreateSampler(const MetalSamplerDescriptor& desc);
	[[nodiscard]] size_t GetCachedSamplerCount() const noexcept;
	void Clear();

private:
	void* m_mtl_device = nullptr;
	std::unordered_map<uint64_t, void*> m_sampler_map; // Hash -> id<MTLSamplerState>
	mutable std::mutex                  m_mutex;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALSAMPLERCACHE_H_
