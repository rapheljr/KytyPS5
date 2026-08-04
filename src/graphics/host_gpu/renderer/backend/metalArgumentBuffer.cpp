// metalArgumentBuffer_stub.cpp
// Non-Apple stub — safe fallback implementations for non-macOS builds.

#include "graphics/host_gpu/renderer/backend/metalArgumentBuffer.h"

#if !defined(__APPLE__)

namespace Libs::Graphics {

MetalArgumentBuffer::MetalArgumentBuffer(void* mtl_device, size_t size_bytes)
    : m_device(mtl_device), m_size_bytes(size_bytes) {}

MetalArgumentBuffer::~MetalArgumentBuffer() = default;

bool MetalArgumentBuffer::EncodeResourceSet(const MetalResourceSet& resources) {
	m_encoded_set = resources;
	return true;
}

bool MetalArgumentBuffer::UpdateDynamicOffset(uint32_t slot, uint64_t new_offset) {
	for (auto& b : m_encoded_set.buffers) {
		if (b.slot == slot) {
			b.offset = new_offset;
			return true;
		}
	}
	return false;
}

MetalArgumentBufferCache::MetalArgumentBufferCache(void* mtl_device, size_t pool_capacity)
    : m_device(mtl_device), m_pool_capacity(pool_capacity) {}

MetalArgumentBufferCache::~MetalArgumentBufferCache() {
	Clear();
}

MetalResourceSet MetalArgumentBufferCache::TranslateNativeDescriptors(
    const DescriptorCache::NativeDescriptors& native,
    const std::vector<uint32_t>& dynamic_offsets) {

	MetalResourceSet set;

	set.buffers.reserve(native.buffers.size() + native.addresses.size());
	for (size_t i = 0; i < native.buffers.size(); ++i) {
		const auto& view = native.buffers[i];
		MetalBufferBinding binding;
		binding.buffer = (void*)(uintptr_t)static_cast<VkBuffer>(view.buffer);
		uint64_t dyn_offset = (i < dynamic_offsets.size()) ? dynamic_offsets[i] : 0;
		binding.offset = view.offset + dyn_offset;
		binding.range  = view.range;
		binding.slot   = static_cast<uint32_t>(i);
		set.buffers.push_back(binding);

		if (view.owner != nullptr) {
			set.lifetime_owners.push_back(view.owner);
		}
	}

	for (size_t i = 0; i < native.addresses.size(); ++i) {
		const auto& view = native.addresses[i];
		MetalBufferBinding binding;
		binding.buffer = (void*)(uintptr_t)static_cast<VkBuffer>(view.buffer);
		binding.offset = view.offset;
		binding.range  = view.range;
		binding.slot   = static_cast<uint32_t>(native.buffers.size() + i);
		set.buffers.push_back(binding);

		if (view.owner != nullptr) {
			set.lifetime_owners.push_back(view.owner);
		}
	}

	set.textures.reserve(native.images.size());
	for (size_t i = 0; i < native.images.size(); ++i) {
		const auto& tex = native.images[i];
		MetalTextureBinding binding;
		binding.texture = (void*)(uintptr_t)static_cast<VkImageView>(tex.image_view);
		binding.slot    = static_cast<uint32_t>(i);
		set.textures.push_back(binding);
	}

	set.samplers.reserve(native.samplers.size());
	for (size_t i = 0; i < native.samplers.size(); ++i) {
		MetalSamplerBinding binding;
		binding.sampler = (void*)(uintptr_t)static_cast<VkSampler>(native.samplers[i]);
		binding.slot    = static_cast<uint32_t>(i);
		set.samplers.push_back(binding);
	}

	return set;
}

MetalArgumentBuffer* MetalArgumentBufferCache::GetOrCreateArgumentBuffer(
    const MetalArgumentBufferLayout& layout,
    const MetalResourceSet&           resources) {

	Common::LockGuard lock(m_mutex);

	auto& pool = m_buffers[layout];

	for (auto& buf : pool) {
		if (buf->GetEncodedResourceSet() == resources) {
			++m_hits;
			return buf.get();
		}
	}

	++m_misses;
	auto new_buf = std::make_unique<MetalArgumentBuffer>(m_device, 4096);
	if (!new_buf->EncodeResourceSet(resources)) {
		return nullptr;
	}

	++m_total_buffers_created;
	MetalArgumentBuffer* ptr = new_buf.get();
	pool.push_back(std::move(new_buf));
	return ptr;
}

void MetalArgumentBufferCache::Clear() {
	Common::LockGuard lock(m_mutex);
	m_buffers.clear();
	m_total_buffers_created = 0;
}

} // namespace Libs::Graphics

#endif // !defined(__APPLE__)
