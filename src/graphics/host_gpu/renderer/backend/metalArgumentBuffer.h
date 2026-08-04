#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALARGUMENTBUFFER_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALARGUMENTBUFFER_H_

// MetalArgumentBuffer — Apple Silicon Performance Optimized
//
// Encapsulates Apple Metal Argument Buffers (MTLArgumentBuffer) and resource binding translation.
//
// Features:
//   - O(1) FNV-1a Hash Map indexing over MetalResourceSets
//   - Base resource hash matching & dynamic offset in-place update reuse
//   - Argument Buffer allocation & encoding via MTLArgumentEncoder / MTLBuffer
//   - Texture, Sampler, and Buffer binding (+ dynamic offsets)
//   - Descriptor Translation: converts engine/Vulkan NativeDescriptors into Metal ResourceSets
//   - Resource Lifetime Management: preserves C++ resource owner shared_ptrs until GPU completes work

#include "common/common.h"
#include "common/threads.h"
#include "graphics/host_gpu/renderer/pipeline/descriptorCache.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

enum class MetalResourceType : uint8_t {
	Buffer,
	Texture,
	Sampler,
};

struct MetalBufferBinding {
	void*    buffer = nullptr; // id<MTLBuffer>
	uint64_t offset = 0;       // Static or dynamic byte offset
	uint64_t range  = 0;       // Byte range
	uint32_t slot   = 0;       // Argument buffer index
};

struct MetalTextureBinding {
	void*    texture = nullptr; // id<MTLTexture>
	uint32_t slot    = 0;       // Argument buffer index
};

struct MetalSamplerBinding {
	void*    sampler = nullptr; // id<MTLSamplerState>
	uint32_t slot    = 0;       // Argument buffer index
};

/// Set of all bound resources for a descriptor layout
struct MetalResourceSet {
	std::vector<MetalBufferBinding>    buffers;
	std::vector<MetalTextureBinding>   textures;
	std::vector<MetalSamplerBinding>   samplers;
	std::vector<std::shared_ptr<void>> lifetime_owners; // Retains host C++ owners

	void Clear() {
		buffers.clear();
		textures.clear();
		samplers.clear();
		lifetime_owners.clear();
	}

	[[nodiscard]] uint64_t ComputeHash() const noexcept {
		uint64_t hash = 14695981039346656037ull;
		auto add_word = [&hash](uint64_t val) {
			hash ^= val;
			hash *= 1099511628211ull;
		};
		for (const auto& b : buffers) {
			add_word(reinterpret_cast<uint64_t>(b.buffer));
			add_word(b.offset);
			add_word(b.range);
			add_word(static_cast<uint64_t>(b.slot));
		}
		for (const auto& t : textures) {
			add_word(reinterpret_cast<uint64_t>(t.texture));
			add_word(static_cast<uint64_t>(t.slot));
		}
		for (const auto& s : samplers) {
			add_word(reinterpret_cast<uint64_t>(s.sampler));
			add_word(static_cast<uint64_t>(s.slot));
		}
		return hash;
	}

	[[nodiscard]] uint64_t ComputeBaseHash() const noexcept {
		uint64_t hash = 14695981039346656037ull;
		auto add_word = [&hash](uint64_t val) {
			hash ^= val;
			hash *= 1099511628211ull;
		};
		for (const auto& b : buffers) {
			add_word(reinterpret_cast<uint64_t>(b.buffer));
			add_word(b.range);
			add_word(static_cast<uint64_t>(b.slot));
		}
		for (const auto& t : textures) {
			add_word(reinterpret_cast<uint64_t>(t.texture));
			add_word(static_cast<uint64_t>(t.slot));
		}
		for (const auto& s : samplers) {
			add_word(reinterpret_cast<uint64_t>(s.sampler));
			add_word(static_cast<uint64_t>(s.slot));
		}
		return hash;
	}

	bool operator==(const MetalResourceSet& other) const noexcept {
		if (buffers.size() != other.buffers.size() ||
		    textures.size() != other.textures.size() ||
		    samplers.size() != other.samplers.size()) {
			return false;
		}
		for (size_t i = 0; i < buffers.size(); ++i) {
			if (buffers[i].buffer != other.buffers[i].buffer ||
			    buffers[i].offset != other.buffers[i].offset ||
			    buffers[i].range != other.buffers[i].range ||
			    buffers[i].slot != other.buffers[i].slot) {
				return false;
			}
		}
		for (size_t i = 0; i < textures.size(); ++i) {
			if (textures[i].texture != other.textures[i].texture ||
			    textures[i].slot != other.textures[i].slot) {
				return false;
			}
		}
		for (size_t i = 0; i < samplers.size(); ++i) {
			if (samplers[i].sampler != other.samplers[i].sampler ||
			    samplers[i].slot != other.samplers[i].slot) {
				return false;
			}
		}
		return true;
	}
};

struct MetalArgumentBufferLayout {
	uint32_t buffer_count  = 0;
	uint32_t texture_count = 0;
	uint32_t sampler_count = 0;

	bool operator==(const MetalArgumentBufferLayout& other) const noexcept {
		return buffer_count == other.buffer_count &&
		       texture_count == other.texture_count &&
		       sampler_count == other.sampler_count;
	}
};

struct MetalArgumentBufferLayoutHash {
	std::size_t operator()(const MetalArgumentBufferLayout& layout) const noexcept {
		std::size_t hash = 0;
		hash ^= std::hash<uint32_t>()(layout.buffer_count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(layout.texture_count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		hash ^= std::hash<uint32_t>()(layout.sampler_count) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
		return hash;
	}
};

class MetalArgumentBuffer final {
public:
	explicit MetalArgumentBuffer(void* mtl_device, size_t size_bytes = 4096);
	~MetalArgumentBuffer();

	KYTY_CLASS_NO_COPY(MetalArgumentBuffer);

	[[nodiscard]] bool IsValid() const noexcept { return m_buffer != nullptr; }
	[[nodiscard]] void* GetMTLBuffer() const noexcept { return m_buffer; }
	[[nodiscard]] size_t GetSizeBytes() const noexcept { return m_size_bytes; }

	bool EncodeResourceSet(const MetalResourceSet& resources);
	bool UpdateDynamicOffset(uint32_t slot, uint64_t new_offset);

	[[nodiscard]] const MetalResourceSet& GetEncodedResourceSet() const noexcept {
		return m_encoded_set;
	}

private:
	void*            m_device     = nullptr;
	void*            m_buffer     = nullptr;
	size_t           m_size_bytes = 0;
	MetalResourceSet m_encoded_set;
};

class MetalArgumentBufferCache final {
public:
	explicit MetalArgumentBufferCache(void* mtl_device, size_t pool_capacity = 256);
	~MetalArgumentBufferCache();

	KYTY_CLASS_NO_COPY(MetalArgumentBufferCache);

	[[nodiscard]] MetalResourceSet TranslateNativeDescriptors(
	    const DescriptorCache::NativeDescriptors& native,
	    const std::vector<uint32_t>& dynamic_offsets = {});

	MetalArgumentBuffer* GetOrCreateArgumentBuffer(const MetalArgumentBufferLayout& layout,
	                                               const MetalResourceSet&           resources);

	void Clear();

	[[nodiscard]] size_t GetCacheSize() const noexcept { return m_total_buffers_created; }
	[[nodiscard]] uint64_t GetHits() const noexcept { return m_hits; }
	[[nodiscard]] uint64_t GetMisses() const noexcept { return m_misses; }
	[[nodiscard]] size_t GetPoolCapacity() const noexcept { return m_pool_capacity; }
	[[nodiscard]] double GetHitRate() const noexcept {
		const uint64_t total = m_hits + m_misses;
		return (total > 0) ? (static_cast<double>(m_hits) / static_cast<double>(total)) * 100.0 : 0.0;
	}

private:
	void*  m_device                = nullptr;
	size_t m_pool_capacity        = 256;
	size_t m_total_buffers_created = 0;

	// O(1) Hash Map indices: exact match & base resource match for dynamic offset updates
	std::unordered_map<uint64_t, std::vector<std::unique_ptr<MetalArgumentBuffer>>> m_hash_index;
	std::unordered_map<uint64_t, MetalArgumentBuffer*>                             m_base_hash_index;

	uint64_t m_hits   = 0;
	uint64_t m_misses = 0;

	mutable Common::Mutex m_mutex;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALARGUMENTBUFFER_H_
