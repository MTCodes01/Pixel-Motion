#include "TextureManager.h"
#include "DX11Device.h"
#include "core/Logger.h"

namespace PixelMotion {

TextureManager::TextureManager()
    : m_width(0)
    , m_height(0)
    , m_initialized(false)
{
}

TextureManager::~TextureManager() {
    Shutdown();
}

bool TextureManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    m_initialized = true;
    return true;
}

void TextureManager::Shutdown() {
    m_texture.Reset();
    m_initialized = false;
}

ID3D11Texture2D* TextureManager::UpdateTexture(int width, int height, const void* data) {
    if (!m_initialized) {
        return nullptr;
    }

    // Create new texture if size changed or doesn't exist
    if (!m_texture || m_width != width || m_height != height) {
        m_texture.Reset();
        m_width = width;
        m_height = height;

        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = width;
        texDesc.Height = height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = DX11Device::GetInstance().GetDevice()->CreateTexture2D(
            &texDesc, nullptr, &m_texture);
        
        if (FAILED(hr)) {
            Logger::Error("Failed to create texture");
            return nullptr;
        }
    }

    // Update texture data
    if (data) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = DX11Device::GetInstance().GetContext()->Map(
            m_texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        
        if (SUCCEEDED(hr)) {
            const uint8_t* src = static_cast<const uint8_t*>(data);
            uint8_t* dst = static_cast<uint8_t*>(mapped.pData);
            
            for (int y = 0; y < height; ++y) {
                memcpy(dst, src, width * 4); // 4 bytes per pixel (BGRA)
                src += width * 4;
                dst += mapped.RowPitch;
            }

            DX11Device::GetInstance().GetContext()->Unmap(m_texture.Get(), 0);
        }
    }

    return m_texture.Get();
}

} // namespace PixelMotion
