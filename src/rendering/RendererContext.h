#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <memory>
#include <map>

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

    void SetVideoTexture(ID3D11Texture2D* texture, int arrayIndex = 0, int contentWidth = 0, int contentHeight = 0);
    void SetScalingMode(int mode); // 0=Fill, 1=Fit, 2=Stretch, 3=Center
    ID3D11Device* GetDevice();

private:
    bool CreateSwapChain(HWND hwnd, int width, int height);
    bool CreateRenderTarget();
    bool LoadShaders();
    bool CreateVertexBuffer();
    void UpdateVertexBuffer(); // Recalculate vertices based on scaling mode

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

    // Per-monitor Video Processor resources for D3D11VA hardware conversion
    ComPtr<ID3D11Texture2D> m_procRgbaTexture;
    ComPtr<ID3D11ShaderResourceView> m_procRgbaSRV;
    ComPtr<ID3D11VideoDevice> m_videoDevice;
    ComPtr<ID3D11VideoContext> m_videoContext;
    ComPtr<ID3D11VideoProcessor> m_videoProcessor;
    ComPtr<ID3D11VideoProcessorEnumerator> m_processorEnumerator;
    ComPtr<ID3D11VideoProcessorOutputView> m_outputView;
    std::map<UINT, ComPtr<ID3D11VideoProcessorInputView>> m_inputViews;
    ID3D11Texture2D* m_lastSourceTexture;
    UINT m_procWidth;
    UINT m_procHeight;

    int m_scalingMode; // 0=Fill, 1=Fit, 2=Stretch, 3=Center
    int m_videoWidth;
    int m_videoHeight;

    bool m_initialized;
};

} // namespace PixelMotion
