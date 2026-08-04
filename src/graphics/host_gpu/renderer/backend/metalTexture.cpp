#include "graphics/host_gpu/renderer/backend/metalTexture.h"

namespace Libs::Graphics {

#if !defined(__APPLE__)

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

bool MetalTexture::Create(void* /*mtl_device*/, const MetalTextureDescriptor& /*desc*/) {
	return false;
}

void MetalTexture::Destroy() {
	m_texture = nullptr;
}

bool MetalTexture::WritePixels(const void* /*pixel_bytes*/, uint64_t /*bytes_per_row*/, uint32_t /*mip_level*/, uint32_t /*layer*/) {
	return false;
}

bool MetalTexture::ReadPixels(void* /*dst_bytes*/, uint64_t /*bytes_per_row*/, uint32_t /*mip_level*/, uint32_t /*layer*/) {
	return false;
}

MetalTextureView MetalTexture::CreateView(MetalPixelFormat /*format*/, MetalTextureType /*type*/, uint32_t /*base_mip*/, uint32_t /*mip_count*/, uint32_t /*base_layer*/, uint32_t /*layer_count*/) {
	return MetalTextureView(nullptr);
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
	m_texture_view = nullptr;
}

#endif // !defined(__APPLE__)

} // namespace Libs::Graphics
