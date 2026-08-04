#include "graphics/host_gpu/renderer/backend/metalArgumentBuffer.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#endif

#include <algorithm>
#include <cstring>

namespace Libs::Graphics {

// ─────────────────────────────────────────────────────────────────────────────
// MetalArgumentBuffer Implementation
// ─────────────────────────────────────────────────────────────────────────────

MetalArgumentBuffer::MetalArgumentBuffer(void* mtl_device, size_t size_bytes)
    : m_device(mtl_device), m_size_bytes(size_bytes) {
#if defined(__APPLE__)
	if (m_device == nullptr) {
		return;
	}
	id<MTLDevice> dev = (__bridge id<MTLDevice>)m_device;
	id<MTLBuffer> buf = [dev newBufferWithLength:m_size_bytes options:MTLResourceStorageModeShared];
	if (buf != nil) {
		m_buffer = (void*)CFBridgingRetain(buf);
	}
#endif
}

MetalArgumentBuffer::~MetalArgumentBuffer() {
#if defined(__APPLE__)
	if (m_buffer != nullptr) {
		CFBridgingRelease(m_buffer);
		m_buffer = nullptr;
	}
#endif
}

bool MetalArgumentBuffer::EncodeResourceSet(const MetalResourceSet& resources) {
	m_encoded_set = resources;
#if defined(__APPLE__)
	if (m_buffer == nullptr) {
		return false;
	}

	id<MTLBuffer> buf = (__bridge id<MTLBuffer>)m_buffer;
	uint8_t* ptr      = static_cast<uint8_t*>([buf contents]);
	if (ptr == nullptr) {
		return false;
	}

	// Layout packing in argument buffer memory:
	// Header: [uint32_t num_buffers][uint32_t num_textures][uint32_t num_samplers]
	// Buffer descriptors: array of {uint64_t handle, uint64_t offset, uint64_t range}
	// Texture descriptors: array of {uint64_t handle}
	// Sampler descriptors: array of {uint64_t handle}

	struct Header {
		uint32_t num_buffers;
		uint32_t num_textures;
		uint32_t num_samplers;
	};

	struct BufferEntry {
		uint64_t handle;
		uint64_t offset;
		uint64_t range;
		uint32_t slot;
	};

	struct HandleEntry {
		uint64_t handle;
		uint32_t slot;
	};

	Header hdr {
		static_cast<uint32_t>(resources.buffers.size()),
		static_cast<uint32_t>(resources.textures.size()),
		static_cast<uint32_t>(resources.samplers.size())
	};

	size_t offset = 0;
	if (offset + sizeof(Header) <= m_size_bytes) {
		std::memcpy(ptr + offset, &hdr, sizeof(Header));
		offset += sizeof(Header);
	}

	for (const auto& b : resources.buffers) {
		if (offset + sizeof(BufferEntry) <= m_size_bytes) {
			BufferEntry entry {
				reinterpret_cast<uint64_t>(b.buffer),
				b.offset,
				b.range,
				b.slot
			};
			std::memcpy(ptr + offset, &entry, sizeof(BufferEntry));
			offset += sizeof(BufferEntry);
		}
	}

	for (const auto& t : resources.textures) {
		if (offset + sizeof(HandleEntry) <= m_size_bytes) {
			HandleEntry entry {
				reinterpret_cast<uint64_t>(t.texture),
				t.slot
			};
			std::memcpy(ptr + offset, &entry, sizeof(HandleEntry));
			offset += sizeof(HandleEntry);
		}
	}

	for (const auto& s : resources.samplers) {
		if (offset + sizeof(HandleEntry) <= m_size_bytes) {
			HandleEntry entry {
				reinterpret_cast<uint64_t>(s.sampler),
				s.slot
			};
			std::memcpy(ptr + offset, &entry, sizeof(HandleEntry));
			offset += sizeof(HandleEntry);
		}
	}

	return true;
#else
	return true;
#endif
}

bool MetalArgumentBuffer::UpdateDynamicOffset(uint32_t slot, uint64_t new_offset) {
	bool updated = false;
	for (auto& b : m_encoded_set.buffers) {
		if (b.slot == slot) {
			b.offset = new_offset;
			updated  = true;
			break;
		}
	}

	if (!updated) {
		return false;
	}

	return EncodeResourceSet(m_encoded_set);
}

// ─────────────────────────────────────────────────────────────────────────────
// MetalArgumentBufferCache Implementation
// ─────────────────────────────────────────────────────────────────────────────

MetalArgumentBufferCache::MetalArgumentBufferCache(void* mtl_device, size_t pool_capacity)
    : m_device(mtl_device), m_pool_capacity(pool_capacity) {}

MetalArgumentBufferCache::~MetalArgumentBufferCache() {
	Clear();
}

MetalResourceSet MetalArgumentBufferCache::TranslateNativeDescriptors(
    const DescriptorCache::NativeDescriptors& native,
    const std::vector<uint32_t>& dynamic_offsets) {

	MetalResourceSet set;

	// 1. Translate Buffer Views
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

	// 2. Translate Texture Views
	set.textures.reserve(native.images.size());
	for (size_t i = 0; i < native.images.size(); ++i) {
		const auto& tex = native.images[i];
		MetalTextureBinding binding;
		binding.texture = (void*)(uintptr_t)static_cast<VkImageView>(tex.image_view);
		binding.slot    = static_cast<uint32_t>(i);
		set.textures.push_back(binding);
	}

	// 3. Translate Samplers
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

	// Search for existing pooled argument buffer with matching encoded resources
	for (auto& buf : pool) {
		if (buf->GetEncodedResourceSet() == resources) {
			++m_hits;
			return buf.get();
		}
	}

	// Cache Miss: allocate new argument buffer
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
