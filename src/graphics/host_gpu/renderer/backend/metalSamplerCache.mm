#include "graphics/host_gpu/renderer/backend/metalSamplerCache.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

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

static MTLSamplerMinMagFilter ToMTLMinMagFilter(MetalMinMagFilter filter) {

	switch (filter) {
		case MetalMinMagFilter::Nearest: return MTLSamplerMinMagFilterNearest;
		case MetalMinMagFilter::Linear:  return MTLSamplerMinMagFilterLinear;
	}
	return MTLSamplerMinMagFilterLinear;
}

static MTLSamplerMipFilter ToMTLMipFilter(MetalMipFilter filter) {
	switch (filter) {
		case MetalMipFilter::NotMipmapped: return MTLSamplerMipFilterNotMipmapped;
		case MetalMipFilter::Nearest:      return MTLSamplerMipFilterNearest;
		case MetalMipFilter::Linear:       return MTLSamplerMipFilterLinear;
	}
	return MTLSamplerMipFilterLinear;
}

static MTLSamplerAddressMode ToMTLAddressMode(MetalSamplerAddressMode mode) {
	switch (mode) {
		case MetalSamplerAddressMode::ClampToEdge:  return MTLSamplerAddressModeClampToEdge;
		case MetalSamplerAddressMode::Repeat:       return MTLSamplerAddressModeRepeat;
		case MetalSamplerAddressMode::MirrorRepeat: return MTLSamplerAddressModeMirrorRepeat;
		case MetalSamplerAddressMode::ClampToZero:  return MTLSamplerAddressModeClampToZero;
	}
	return MTLSamplerAddressModeClampToEdge;
}

static MTLCompareFunction ToMTLCompareFunction(MetalCompareFunction func) {
	switch (func) {
		case MetalCompareFunction::Never:        return MTLCompareFunctionNever;
		case MetalCompareFunction::Less:         return MTLCompareFunctionLess;
		case MetalCompareFunction::Equal:        return MTLCompareFunctionEqual;
		case MetalCompareFunction::LessEqual:    return MTLCompareFunctionLessEqual;
		case MetalCompareFunction::Greater:      return MTLCompareFunctionGreater;
		case MetalCompareFunction::NotEqual:     return MTLCompareFunctionNotEqual;
		case MetalCompareFunction::GreaterEqual: return MTLCompareFunctionGreaterEqual;
		case MetalCompareFunction::Always:       return MTLCompareFunctionAlways;
	}
	return MTLCompareFunctionNever;
}

MetalSamplerCache::~MetalSamplerCache() {
	Shutdown();
}

bool MetalSamplerCache::Initialize(void* mtl_device) {
	if (mtl_device == nullptr) {
		return false;
	}
	Shutdown();
	m_mtl_device = mtl_device;
	return true;
}

void MetalSamplerCache::Shutdown() {
	Clear();
	m_mtl_device = nullptr;
}

void* MetalSamplerCache::GetOrCreateSampler(const MetalSamplerDescriptor& desc) {
	if (m_mtl_device == nullptr) {
		return nullptr;
	}

	const uint64_t hash = desc.ComputeHash();

	std::lock_guard<std::mutex> lock(m_mutex);
	auto it = m_sampler_map.find(hash);
	if (it != m_sampler_map.end()) {
		return it->second;
	}

	id<MTLDevice> device = (__bridge id<MTLDevice>)m_mtl_device;

	MTLSamplerDescriptor* mtl_desc = [[MTLSamplerDescriptor alloc] init];
	mtl_desc.minFilter             = ToMTLMinMagFilter(desc.min_filter);
	mtl_desc.magFilter             = ToMTLMinMagFilter(desc.mag_filter);
	mtl_desc.mipFilter             = ToMTLMipFilter(desc.mip_filter);
	mtl_desc.sAddressMode          = ToMTLAddressMode(desc.address_s);
	mtl_desc.tAddressMode          = ToMTLAddressMode(desc.address_t);
	mtl_desc.rAddressMode          = ToMTLAddressMode(desc.address_r);
	mtl_desc.maxAnisotropy         = static_cast<NSUInteger>(desc.max_anisotropy);
	mtl_desc.lodMinClamp           = desc.lod_min;
	mtl_desc.lodMaxClamp           = desc.lod_max;
	mtl_desc.compareFunction       = ToMTLCompareFunction(desc.compare_func);
	mtl_desc.normalizedCoordinates = desc.normalized_coords ? YES : NO;

	id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:mtl_desc];
	if (sampler == nil) {
		return nullptr;
	}

	void* retained_sampler = (void*)CFBridgingRetain(sampler);
	m_sampler_map[hash]    = retained_sampler;

	return retained_sampler;
}

size_t MetalSamplerCache::GetCachedSamplerCount() const noexcept {
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_sampler_map.size();
}

void MetalSamplerCache::Clear() {
	std::lock_guard<std::mutex> lock(m_mutex);
	for (auto& [hash, ptr] : m_sampler_map) {
		if (ptr != nullptr) {
			CFBridgingRelease(ptr);
		}
	}
	m_sampler_map.clear();
}

} // namespace Libs::Graphics

#endif // __APPLE__
