#pragma once

#include <d3d11.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace PixelMotion {

/**
 * Texture manager for video frames
 * Handles GPU texture uploads and management
 */
class TextureManager {
public:
    TextureManager();
    ~TextureManager();

    bool Initialize();
    void Shutdown();

    /**
     * Create or update texture from video frame data
     * @param width Frame width
     * @param height Frame height
     * @param data Frame data (BGRA format)
     * @return Texture pointer
     */
    ID3D11Texture2D* UpdateTexture(int width, int height, const void* data);

private:
    ComPtr<ID3D11Texture2D> m_texture;
    int m_width;
    int m_height;
    bool m_initialized;
};

} // namespace PixelMotion
