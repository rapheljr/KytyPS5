#include "graphics/host_gpu/renderer/backend/metalSamplerCache.h"

#include <cstring>

namespace Libs::Graphics {

uint64_t MetalSamplerDescriptor::ComputeHash() const noexcept {
	uint64_t hash = 14695981039346656037ULL; // 64-bit FNV-1a offset basis
	auto hash_bytes = [&hash](const void* data, size_t size) {
		const auto* bytes = static_cast<const uint8_t*>(data);
		for (size_t i = 0; i < size; ++i) {
			hash ^= bytes[i];
			hash *= 1099511628211ULL; // FNV prime
		}
	};

	hash_bytes(&min_filter, sizeof(min_filter));
	hash_bytes(&mag_filter, sizeof(mag_filter));
	hash_bytes(&mip_filter, sizeof(mip_filter));
	hash_bytes(&address_s, sizeof(address_s));
	hash_bytes(&address_t, sizeof(address_t));
	hash_bytes(&address_r, sizeof(address_r));
	hash_bytes(&max_anisotropy, sizeof(max_anisotropy));
	hash_bytes(&lod_min, sizeof(lod_min));
	hash_bytes(&lod_max, sizeof(lod_max));
	hash_bytes(&compare_func, sizeof(compare_func));
	hash_bytes(&normalized_coords, sizeof(normalized_coords));

	return hash;
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

#if !defined(__APPLE__)

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

#endif // !defined(__APPLE__)

} // namespace Libs::Graphics
