#include "graphics/host_gpu/renderer/backend/metalTexture.h"

#if defined(__APPLE__)
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

namespace Libs::Graphics {

static MTLPixelFormat ToMTLPixelFormat(MetalPixelFormat format) {
	switch (format) {
		case MetalPixelFormat::R8Unorm:               return MTLPixelFormatR8Unorm;
		case MetalPixelFormat::RGBA8Unorm:            return MTLPixelFormatRGBA8Unorm;
		case MetalPixelFormat::BGRA8Unorm:            return MTLPixelFormatBGRA8Unorm;
		case MetalPixelFormat::BGRA8Unorm_sRGB:       return MTLPixelFormatBGRA8Unorm_sRGB;
		case MetalPixelFormat::RGBA16Float:          return MTLPixelFormatRGBA16Float;
		case MetalPixelFormat::Depth32Float:          return MTLPixelFormatDepth32Float;
		case MetalPixelFormat::Depth24Unorm_Stencil8: return MTLPixelFormatDepth24Unorm_Stencil8;
		case MetalPixelFormat::Depth32Float_Stencil8: return MTLPixelFormatDepth32Float_Stencil8;
		default:                                      return MTLPixelFormatRGBA8Unorm;
	}
}

static MTLTextureType ToMTLTextureType(MetalTextureType type) {
	switch (type) {
		case MetalTextureType::Texture1D:      return MTLTextureType1D;
		case MetalTextureType::Texture2D:      return MTLTextureType2D;
		case MetalTextureType::Texture3D:      return MTLTextureType3D;
		case MetalTextureType::TextureCube:    return MTLTextureTypeCube;
		case MetalTextureType::Texture2DArray: return MTLTextureType2DArray;
		case MetalTextureType::Depth2D:        return MTLTextureType2D;
		case MetalTextureType::MSAA2D:         return MTLTextureType2DMultisample;
	}
	return MTLTextureType2D;
}

static MTLTextureUsage ToMTLTextureUsage(MetalTextureUsage usage) {
	MTLTextureUsage flags = MTLTextureUsageUnknown;
	if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(MetalTextureUsage::ShaderRead)) {
		flags |= MTLTextureUsageShaderRead;
	}
	if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(MetalTextureUsage::ShaderWrite)) {
		flags |= MTLTextureUsageShaderWrite;
	}
	if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(MetalTextureUsage::RenderTarget)) {
		flags |= MTLTextureUsageRenderTarget;
	}
	if (static_cast<uint32_t>(usage) & static_cast<uint32_t>(MetalTextureUsage::PixelFormatView)) {
		flags |= MTLTextureUsagePixelFormatView;
	}
	return flags;
}

static MTLStorageMode ToMTLStorageMode(MetalBufferMemoryType mem_type) {
	switch (mem_type) {
		case MetalBufferMemoryType::Shared:   return MTLStorageModeShared;
		case MetalBufferMemoryType::Private:  return MTLStorageModePrivate;
		case MetalBufferMemoryType::Managed:
#if defined(TARGET_OS_OSX) && !defined(TARGET_OS_IPHONE)
			return MTLStorageModeManaged;
#else
			return MTLStorageModeShared;
#endif
		case MetalBufferMemoryType::Staging:  return MTLStorageModeShared;
	}
	return MTLStorageModePrivate;
}

MetalTexture::~MetalTexture() {
	Destroy();
}

MetalTexture::MetalTexture(MetalTexture&& other) noexcept
	: m_texture(other.m_texture)
	, m_desc(other.m_desc) {
	other.m_texture = nullptr;
}

MetalTexture& MetalTexture::operator=(MetalTexture&& other) noexcept {
	if (this != &other) {
		Destroy();
		m_texture = other.m_texture;
		m_desc = other.m_desc;
		other.m_texture = nullptr;
	}
	return *this;
}

bool MetalTexture::Create(void* mtl_device, const MetalTextureDescriptor& desc) {
	if (mtl_device == nullptr || desc.width == 0 || desc.height == 0) {
		return false;
	}
	Destroy();

	id<MTLDevice> device = (__bridge id<MTLDevice>)mtl_device;

	MTLTextureDescriptor* mtl_desc = [[MTLTextureDescriptor alloc] init];
	mtl_desc.textureType     = ToMTLTextureType(desc.type);
	mtl_desc.pixelFormat     = ToMTLPixelFormat(desc.format);
	mtl_desc.width           = static_cast<NSUInteger>(desc.width);
	mtl_desc.height          = static_cast<NSUInteger>(desc.height);
	mtl_desc.depth           = static_cast<NSUInteger>(desc.depth);
	mtl_desc.arrayLength     = static_cast<NSUInteger>(desc.array_layers);
	mtl_desc.mipmapLevelCount = static_cast<NSUInteger>(desc.mip_levels);
	mtl_desc.sampleCount     = static_cast<NSUInteger>(desc.sample_count);
	mtl_desc.usage           = ToMTLTextureUsage(desc.usage);
	mtl_desc.storageMode     = ToMTLStorageMode(desc.mem_type);

	id<MTLTexture> texture = [device newTextureWithDescriptor:mtl_desc];
	if (texture == nil) {
		return false;
	}

	m_texture = (void*)CFBridgingRetain(texture);
	m_desc    = desc;

	return true;
}

void MetalTexture::Destroy() {
	if (m_texture != nullptr) {
		CFBridgingRelease(m_texture);
		m_texture = nullptr;
	}
	m_desc = {};
}

bool MetalTexture::WritePixels(const void* pixel_bytes, uint64_t bytes_per_row, uint32_t mip_level, uint32_t layer) {
	if (m_texture == nullptr || pixel_bytes == nullptr) {
		return false;
	}
	id<MTLTexture> texture = (__bridge id<MTLTexture>)m_texture;

	uint32_t mip_w = std::max<uint32_t>(1, m_desc.width >> mip_level);
	uint32_t mip_h = std::max<uint32_t>(1, m_desc.height >> mip_level);

	MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(mip_w), static_cast<NSUInteger>(mip_h));
	[texture replaceRegion:region
	           mipmapLevel:static_cast<NSUInteger>(mip_level)
	                 slice:static_cast<NSUInteger>(layer)
	             withBytes:pixel_bytes
	           bytesPerRow:static_cast<NSUInteger>(bytes_per_row)
	         bytesPerImage:0];

	return true;
}

bool MetalTexture::ReadPixels(void* dst_bytes, uint64_t bytes_per_row, uint32_t mip_level, uint32_t layer) {
	if (m_texture == nullptr || dst_bytes == nullptr) {
		return false;
	}
	id<MTLTexture> texture = (__bridge id<MTLTexture>)m_texture;

	uint32_t mip_w = std::max<uint32_t>(1, m_desc.width >> mip_level);
	uint32_t mip_h = std::max<uint32_t>(1, m_desc.height >> mip_level);

	MTLRegion region = MTLRegionMake2D(0, 0, static_cast<NSUInteger>(mip_w), static_cast<NSUInteger>(mip_h));
	[texture getBytes:dst_bytes
	      bytesPerRow:static_cast<NSUInteger>(bytes_per_row)
	    bytesPerImage:0
	       fromRegion:region
	      mipmapLevel:static_cast<NSUInteger>(mip_level)
	            slice:static_cast<NSUInteger>(layer)];

	return true;
}

MetalTextureView MetalTexture::CreateView(MetalPixelFormat format, MetalTextureType type, uint32_t base_mip, uint32_t mip_count, uint32_t base_layer, uint32_t layer_count) {
	if (m_texture == nullptr) {
		return MetalTextureView(nullptr);
	}
	id<MTLTexture> texture = (__bridge id<MTLTexture>)m_texture;

	id<MTLTexture> view = [texture newTextureViewWithPixelFormat:ToMTLPixelFormat(format)
	                                                 textureType:ToMTLTextureType(type)
	                                                      levels:NSMakeRange(static_cast<NSUInteger>(base_mip), static_cast<NSUInteger>(mip_count))
	                                                      slices:NSMakeRange(static_cast<NSUInteger>(base_layer), static_cast<NSUInteger>(layer_count))];

	if (view == nil) {
		return MetalTextureView(nullptr);
	}

	return MetalTextureView((void*)CFBridgingRetain(view));
}

MetalTextureView::~MetalTextureView() {
	Destroy();
}

MetalTextureView::MetalTextureView(MetalTextureView&& other) noexcept
	: m_texture_view(other.m_texture_view) {
	other.m_texture_view = nullptr;
}

MetalTextureView& MetalTextureView::operator=(MetalTextureView&& other) noexcept {
	if (this != &other) {
		Destroy();
		m_texture_view = other.m_texture_view;
		other.m_texture_view = nullptr;
	}
	return *this;
}

void MetalTextureView::Destroy() {
	if (m_texture_view != nullptr) {
		CFBridgingRelease(m_texture_view);
		m_texture_view = nullptr;
	}
}

} // namespace Libs::Graphics

#endif // __APPLE__
