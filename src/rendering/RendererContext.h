#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

namespace PixelMotion {

class TextureManager;

/**
 * Per-monitor rendering context
 * Manages swap chain and rendering for a single monitor
 */
class RendererContext {
public:
    RendererContext();
    ~RendererContext();

    bool Initialize(HWND hwnd, int width, int height);
    void Shutdown();

    void Render();
    void Present();

    void SetVideoTexture(ID3D11Texture2D* texture, int arrayIndex = 0);
    ID3D11Device* GetDevice();

private:
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTarget();
    bool LoadShaders();
    bool CreateVertexBuffer();

    HWND m_hwnd;
    int m_width;
    int m_height;

    ComPtr<IDXGISwapChain1> m_swapChain;
    ComPtr<ID3D11RenderTargetView> m_renderTargetView;
    
    ComPtr<ID3D11VertexShader> m_vertexShader;
    ComPtr<ID3D11PixelShader> m_pixelShader;
    ComPtr<ID3D11InputLayout> m_inputLayout;
    ComPtr<ID3D11Buffer> m_vertexBuffer;
    ComPtr<ID3D11SamplerState> m_samplerState;

    ComPtr<ID3D11Texture2D> m_videoTexture;
    ComPtr<ID3D11ShaderResourceView> m_videoSRV;

    bool m_initialized;
};

} // namespace PixelMotion
