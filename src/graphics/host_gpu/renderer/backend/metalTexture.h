#ifndef EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALTEXTURE_H_
#define EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALTEXTURE_H_

#include "common/common.h"
#include "graphics/host_gpu/renderer/backend/metalBuffer.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Libs::Graphics {

enum class MetalTextureType : uint8_t {
	Texture1D,
	Texture2D,
	Texture3D,
	TextureCube,
	Texture2DArray,
	Depth2D,
	MSAA2D
};

enum class MetalPixelFormat : uint32_t {
	Invalid = 0,
	R8Unorm,
	RGBA8Unorm,
	BGRA8Unorm,
	BGRA8Unorm_sRGB,
	RGBA16Float,
	Depth32Float,
	Depth24Unorm_Stencil8,
	Depth32Float_Stencil8
};

enum class MetalTextureUsage : uint32_t {
	Unknown          = 0,
	ShaderRead       = 1 << 0,
	ShaderWrite      = 1 << 1,
	RenderTarget     = 1 << 2,
	PixelFormatView  = 1 << 3
};

inline MetalTextureUsage operator|(MetalTextureUsage a, MetalTextureUsage b) {
	return static_cast<MetalTextureUsage>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct MetalTextureDescriptor {
	MetalTextureType      type         = MetalTextureType::Texture2D;
	MetalPixelFormat      format       = MetalPixelFormat::RGBA8Unorm;
	uint32_t              width        = 1;
	uint32_t              height       = 1;
	uint32_t              depth        = 1;
	uint32_t              array_layers = 1;
	uint32_t              mip_levels   = 1;
	uint32_t              sample_count = 1;
	MetalTextureUsage     usage        = MetalTextureUsage::ShaderRead | MetalTextureUsage::RenderTarget;
	MetalBufferMemoryType mem_type     = MetalBufferMemoryType::Private;
};

class MetalTextureView;

class MetalTexture {
public:
	MetalTexture() = default;
	~MetalTexture();

	MetalTexture(const MetalTexture&) = delete;
	MetalTexture& operator=(const MetalTexture&) = delete;

	MetalTexture(MetalTexture&& other) noexcept;
	MetalTexture& operator=(MetalTexture&& other) noexcept;

	bool Create(void* mtl_device, const MetalTextureDescriptor& desc);
	void Destroy();

	[[nodiscard]] void*                   GetMTLTexture() const noexcept { return m_texture; }
	[[nodiscard]] MetalTextureDescriptor  GetDescriptor() const noexcept { return m_desc; }
	[[nodiscard]] uint32_t                GetWidth() const noexcept { return m_desc.width; }
	[[nodiscard]] uint32_t                GetHeight() const noexcept { return m_desc.height; }
	[[nodiscard]] uint32_t                GetDepth() const noexcept { return m_desc.depth; }
	[[nodiscard]] uint32_t                GetMipLevels() const noexcept { return m_desc.mip_levels; }
	[[nodiscard]] uint32_t                GetArrayLayers() const noexcept { return m_desc.array_layers; }
	[[nodiscard]] MetalPixelFormat        GetPixelFormat() const noexcept { return m_desc.format; }

	bool WritePixels(const void* pixel_bytes, uint64_t bytes_per_row, uint32_t mip_level = 0, uint32_t layer = 0);
	bool ReadPixels(void* dst_bytes, uint64_t bytes_per_row, uint32_t mip_level = 0, uint32_t layer = 0);

	[[nodiscard]] MetalTextureView CreateView(MetalPixelFormat format, MetalTextureType type, uint32_t base_mip = 0, uint32_t mip_count = 1, uint32_t base_layer = 0, uint32_t layer_count = 1);

private:
	void*                  m_texture = nullptr; // id<MTLTexture>
	MetalTextureDescriptor m_desc {};
};

class MetalTextureView {
public:
	MetalTextureView() = default;
	explicit MetalTextureView(void* mtl_texture_view) : m_texture_view(mtl_texture_view) {}
	~MetalTextureView();

	MetalTextureView(const MetalTextureView&) = delete;
	MetalTextureView& operator=(const MetalTextureView&) = delete;

	MetalTextureView(MetalTextureView&& other) noexcept;
	MetalTextureView& operator=(MetalTextureView&& other) noexcept;

	[[nodiscard]] void* GetMTLTextureView() const noexcept { return m_texture_view; }
	void Destroy();

private:
	void* m_texture_view = nullptr;
};

} // namespace Libs::Graphics

#endif // EMULATOR_INCLUDE_EMULATOR_GRAPHICS_RENDERER_BACKEND_METALTEXTURE_H_
