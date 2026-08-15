#include "graphics/host_gpu/renderer/backend/metalSamplerCache.h"
#include <xxhash.h>

#if !defined(__APPLE__)

#include <cstring>

namespace Libs::Graphics {

uint64_t MetalSamplerDescriptor::ComputeHash() const noexcept {
	return XXH3_64bits(this, sizeof(*this));
}

bool MetalSamplerDescriptor::operator==(const MetalSamplerDescriptor& other) const noexcept {
	return min_filter == other.min_filter &&
	       mag_filter == other.mag_filter &&
	       mip_filter == other.mip_filter &&
	       address_s == other.address_s &&
	       address_t == other.address_t &&
	       address_r == other.address_r &&
	       max_anisotropy == other.max_anisotropy &&
	       lod_min == other.lod_min &&
	       lod_max == other.lod_max &&
	       compare_func == other.compare_func &&
	       normalized_coords == other.normalized_coords;
}

MetalSamplerCache::~MetalSamplerCache() {
	Shutdown();
}

bool MetalSamplerCache::Initialize(void* /*mtl_device*/) {
	return false;
}

void MetalSamplerCache::Shutdown() {
	Clear();
}

void* MetalSamplerCache::GetOrCreateSampler(const MetalSamplerDescriptor& /*desc*/) {
	return nullptr;
}

size_t MetalSamplerCache::GetCachedSamplerCount() const noexcept {
	return 0;
}

void MetalSamplerCache::Clear() {
	m_sampler_map.clear();
}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
