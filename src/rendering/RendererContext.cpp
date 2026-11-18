#include "RendererContext.h"
#include "DX11Device.h"
#include "core/Logger.h"

#include <d3dcompiler.h>
#include <d3d11_1.h>
#include <filesystem>
#include <map>

namespace PixelMotion {

// Safe blit wrapper protected with structured exception handling (__try/__except)
// This shields the entire application if an underlying display driver (e.g. amdxx64.dll)
// experiences an internal driver access violation (0xc0000005) during hardware video processing.
static bool SafeVideoProcessorBlt(
    ID3D11VideoContext* videoContext,
    ID3D11VideoProcessor* videoProcessor,
    ID3D11VideoProcessorOutputView* outputView,
    ID3D11VideoProcessorInputView* inputView)
{
    __try {
        D3D11_VIDEO_PROCESSOR_STREAM stream = {};
        stream.Enable = TRUE;
        stream.pInputSurface = inputView;

        HRESULT hr = videoContext->VideoProcessorBlt(videoProcessor, outputView, 0, 1, &stream);
        return SUCCEEDED(hr);
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Simple vertex structure
struct Vertex {
    float position[3];
    float texCoord[2];
};

RendererContext::RendererContext()
    : m_hwnd(nullptr)
    , m_width(0)
    , m_height(0)
    , m_scalingMode(0) // Default to Fill (Crop)
    , m_videoWidth(0)
    , m_videoHeight(0)
    , m_procWidth(0)
    , m_procHeight(0)
    , m_lastSourceTexture(nullptr)
    , m_monitorBounds({})
    , m_virtualBounds({})
    , m_initialized(false)
{
}

RendererContext::~RendererContext() {
    Shutdown();
}

bool RendererContext::Initialize(HWND hwnd, int width, int height) {
    if (m_initialized) {
        return true;
    }

    m_hwnd = hwnd;
    m_width = width;
    m_height = height;

    Logger::Info("Initializing renderer context...");

    // Ensure DX11 device is initialized
    if (!DX11Device::GetInstance().Initialize()) {
        Logger::Error("Failed to initialize DX11 device");
        return false;
    }

    // Create swap chain
    if (!CreateSwapChain(hwnd, width, height)) {
        Logger::Error("Failed to create swap chain");
        return false;
    }

    // Create render target
    if (!CreateRenderTarget()) {
        Logger::Error("Failed to create render target");
        return false;
    }

    // Load shaders
    if (!LoadShaders()) {
        Logger::Error("Failed to load shaders");
        return false;
    }

    // Create vertex buffer
    if (!CreateVertexBuffer()) {
        Logger::Error("Failed to create vertex buffer");
        return false;
    }

    // Create sampler state (Clamp for Fill, Fit, Stretch, Center, Span)
    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    HRESULT hr = DX11Device::GetInstance().GetDevice()->CreateSamplerState(
        &samplerDesc, &m_samplerState);
    if (FAILED(hr)) {
        Logger::Error("Failed to create sampler state");
        return false;
    }

    // Create wrap sampler state (for Tile mode)
    D3D11_SAMPLER_DESC wrapDesc = samplerDesc;
    wrapDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    wrapDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    DX11Device::GetInstance().GetDevice()->CreateSamplerState(
        &wrapDesc, &m_samplerWrap);

    m_initialized = true;
    Logger::Info("Renderer context initialized successfully");
    return true;
}

void RendererContext::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_inputViews.clear();
    m_lastSourceTexture = nullptr;
    m_outputView.Reset();
    m_videoProcessor.Reset();
    m_processorEnumerator.Reset();
    m_videoContext.Reset();
    m_videoDevice.Reset();
    m_procRgbaSRV.Reset();
    m_procRgbaTexture.Reset();
    m_procWidth = 0;
    m_procHeight = 0;

    m_samplerState.Reset();
    m_samplerWrap.Reset();
    m_videoSRV.Reset();
    m_videoTexture.Reset();
    m_vertexBuffer.Reset();
    m_inputLayout.Reset();
    m_pixelShader.Reset();
    m_vertexShader.Reset();
    m_renderTargetView.Reset();
    m_swapChain.Reset();

    m_initialized = false;
}

void RendererContext::ClearVideoResources() {
    m_videoSRV.Reset();
    m_videoTexture.Reset();
    m_inputViews.clear();
    m_lastSourceTexture = nullptr;
    m_outputView.Reset();
    m_videoProcessor.Reset();
    m_processorEnumerator.Reset();
    m_videoContext.Reset();
    m_videoDevice.Reset();
    m_procRgbaSRV.Reset();
    m_procRgbaTexture.Reset();
    m_procWidth = 0;
    m_procHeight = 0;
}

bool RendererContext::CreateSwapChain(HWND hwnd, int width, int height) {
    Logger::Info("Creating swap chain for HWND: " + std::to_string(reinterpret_cast<uintptr_t>(hwnd)) + 
                 ", size: " + std::to_string(width) + "x" + std::to_string(height));
    
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = 0;

    HRESULT hr = DX11Device::GetInstance().GetFactory()->CreateSwapChainForHwnd(
        DX11Device::GetInstance().GetDevice(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &m_swapChain
    );

    if (SUCCEEDED(hr)) {
        Logger::Info("Swap chain created successfully");
    } else {
        Logger::Error("Failed to create swap chain, HRESULT: " + std::to_string(hr));
    }

    return SUCCEEDED(hr);
}

bool RendererContext::CreateRenderTarget() {
    ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), &backBuffer);
    if (FAILED(hr)) {
        return false;
    }

    hr = DX11Device::GetInstance().GetDevice()->CreateRenderTargetView(
        backBuffer.Get(), nullptr, &m_renderTargetView);
    
    return SUCCEEDED(hr);
}

bool RendererContext::LoadShaders() {
    // For now, create placeholder shaders
    // In full implementation, load compiled .cso files
    
    // Simple passthrough vertex shader
    const char* vsSource = R"(
        struct VS_INPUT {
            float3 pos : POSITION;
            float2 tex : TEXCOORD;
        };
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        PS_INPUT main(VS_INPUT input) {
            PS_INPUT output;
            output.pos = float4(input.pos, 1.0f);
            output.tex = input.tex;
            return output;
        }
    )";

    // Simple texture sampling pixel shader
    const char* psSource = R"(
        Texture2D tex : register(t0);
        SamplerState samp : register(s0);
        struct PS_INPUT {
            float4 pos : SV_POSITION;
            float2 tex : TEXCOORD;
        };
        float4 main(PS_INPUT input) : SV_TARGET {
            return tex.Sample(samp, input.tex);
        }
    )";

    ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;

    // Compile vertex shader
    HRESULT hr = D3DCompile(vsSource, strlen(vsSource), nullptr, nullptr, nullptr,
        "main", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Error("VS compilation error: " + std::string((char*)errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = DX11Device::GetInstance().GetDevice()->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
    if (FAILED(hr)) {
        return false;
    }

    // Compile pixel shader
    hr = D3DCompile(psSource, strlen(psSource), nullptr, nullptr, nullptr,
        "main", "ps_5_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            Logger::Error("PS compilation error: " + std::string((char*)errorBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = DX11Device::GetInstance().GetDevice()->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
    if (FAILED(hr)) {
        return false;
    }

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = DX11Device::GetInstance().GetDevice()->CreateInputLayout(
        layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), 
        vsBlob->GetBufferSize(), &m_inputLayout);

    return SUCCEEDED(hr);
}

bool RendererContext::CreateVertexBuffer() {
    // Fullscreen quad vertices (will be updated by UpdateVertexBuffer)
    Vertex vertices[] = {
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },  // Top-left
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },  // Top-right
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },  // Bottom-left
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },  // Bottom-right
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC; // Changed to DYNAMIC for updates
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE; // Allow CPU writes

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    HRESULT hr = DX11Device::GetInstance().GetDevice()->CreateBuffer(
        &bufferDesc, &initData, &m_vertexBuffer);

    return SUCCEEDED(hr);
}

void RendererContext::Render() {
    if (!m_initialized) {
        return;
    }

    auto* context = DX11Device::GetInstance().GetContext();

    // Clear to black
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);

    // Set render target
    context->OMSetRenderTargets(1, m_renderTargetView.GetAddressOf(), nullptr);

    // Set viewport
    D3D11_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(m_width);
    viewport.Height = static_cast<float>(m_height);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);

    // If we have a video texture, render it
    if (m_videoSRV) {
        context->IASetInputLayout(m_inputLayout.Get());
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        
        UINT stride = sizeof(Vertex);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);

        context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
        context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, m_videoSRV.GetAddressOf());
        
        ID3D11SamplerState* sampler = (m_scalingMode == 4 && m_samplerWrap) ? m_samplerWrap.Get() : m_samplerState.Get();
        context->PSSetSamplers(0, 1, &sampler);

        context->Draw(4, 0);
    }
}

void RendererContext::Present() {
    if (m_swapChain) {
        // Frame pacing is accurately controlled by the steady_clock timer in WallpaperWindow.
        // Present(0, 0) avoids GPU VSync spin-wait locks and lets the GPU sleep between frames.
        m_swapChain->Present(0, 0);
    }
}

void RendererContext::SetVideoTexture(ID3D11Texture2D* texture, int arrayIndex, int contentWidth, int contentHeight) {
    if (!texture) {
        m_videoSRV.Reset();
        m_videoWidth = 0;
        m_videoHeight = 0;
        return;
    }

    // Get texture description
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);
    
    // If content dimensions not provided, use texture dimensions
    if (contentWidth <= 0) contentWidth = texDesc.Width;
    if (contentHeight <= 0) contentHeight = texDesc.Height;

    // Update video dimensions and vertex buffer if changed
    bool dimensionsChanged = (m_videoWidth != contentWidth || 
                             m_videoHeight != contentHeight);
    if (dimensionsChanged) {
        m_videoWidth = contentWidth;
        m_videoHeight = contentHeight;
        UpdateVertexBuffer(); // Recalculate vertices for new video size
    }

    auto* device = DX11Device::GetInstance().GetDevice();
    auto* context = DX11Device::GetInstance().GetContext();

    // Check format - if BGRA/RGBA, use directly
    if (texDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || 
        texDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        
        // Clean up any lingering video processor resources from prior video playback to release GPU/RAM
        if (m_procRgbaTexture || m_videoProcessor) {
            m_inputViews.clear();
            m_lastSourceTexture = nullptr;
            m_outputView.Reset();
            m_videoProcessor.Reset();
            m_processorEnumerator.Reset();
            m_videoContext.Reset();
            m_videoDevice.Reset();
            m_procRgbaSRV.Reset();
            m_procRgbaTexture.Reset();
            m_procWidth = 0;
            m_procHeight = 0;
        }

        if (m_videoTexture.Get() == texture && m_videoSRV) {
            return;
        }

        m_videoTexture = texture;
        m_videoSRV.Reset();
        
        // Create SRV directly on the input texture
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = texDesc.Format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;
        
        HRESULT hr = device->CreateShaderResourceView(texture, &srvDesc, &m_videoSRV);
        if (FAILED(hr)) {
            Logger::Error("Failed to create SRV for RGBA texture: " + std::to_string(hr));
        }
        return;
    }
    
    // Only continue with D3D11VA/Video Processor conversion for NV12
    if (texDesc.Format != DXGI_FORMAT_NV12) {
        // Fallback or unknown format
        Logger::Warning("Unexpected texture format in SetVideoTexture: " + std::to_string(texDesc.Format));
        return;
    }

    // Check if we need to create/recreate resources for new video dimensions
    if (texDesc.Width != m_procWidth || texDesc.Height != m_procHeight || !m_videoProcessor) {
        m_procWidth = texDesc.Width;
        m_procHeight = texDesc.Height;
        
        Logger::Info("Creating video processing resources for " + 
                     std::to_string(m_procWidth) + "x" + std::to_string(m_procHeight));
        
        // Reset old resources
        m_inputViews.clear();
        m_lastSourceTexture = nullptr;
        m_outputView.Reset();
        m_videoProcessor.Reset();
        m_processorEnumerator.Reset();
        m_videoContext.Reset();
        m_videoDevice.Reset();
        m_procRgbaSRV.Reset();
        m_procRgbaTexture.Reset();

        // Create an RGBA texture for rendering
        D3D11_TEXTURE2D_DESC rgbaDesc = {};
        rgbaDesc.Width = texDesc.Width;
        rgbaDesc.Height = texDesc.Height;
        rgbaDesc.MipLevels = 1;
        rgbaDesc.ArraySize = 1;
        rgbaDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        rgbaDesc.SampleDesc.Count = 1;
        rgbaDesc.Usage = D3D11_USAGE_DEFAULT;
        rgbaDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        
        HRESULT hr = device->CreateTexture2D(&rgbaDesc, nullptr, &m_procRgbaTexture);
        if (FAILED(hr)) {
            Logger::Error("Failed to create RGBA texture: " + std::to_string(hr));
            return;
        }
        
        // Create SRV for the RGBA texture
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        hr = device->CreateShaderResourceView(m_procRgbaTexture.Get(), &srvDesc, &m_procRgbaSRV);
        if (FAILED(hr)) {
            Logger::Error("Failed to create SRV: " + std::to_string(hr));
            return;
        }
        
        // Get video device interface
        hr = device->QueryInterface(__uuidof(ID3D11VideoDevice), &m_videoDevice);
        if (FAILED(hr)) {
            Logger::Error("Failed to get ID3D11VideoDevice: " + std::to_string(hr));
            return;
        }
        
        hr = context->QueryInterface(__uuidof(ID3D11VideoContext), &m_videoContext);
        if (FAILED(hr)) {
            Logger::Error("Failed to get ID3D11VideoContext: " + std::to_string(hr));
            return;
        }
        
        // Create video processor
        D3D11_VIDEO_PROCESSOR_CONTENT_DESC contentDesc = {};
        contentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
        contentDesc.InputWidth = texDesc.Width;
        contentDesc.InputHeight = texDesc.Height;
        contentDesc.OutputWidth = texDesc.Width;
        contentDesc.OutputHeight = texDesc.Height;
        contentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
        
        hr = m_videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &m_processorEnumerator);
        if (FAILED(hr)) {
            Logger::Error("Failed to create video processor enumerator: " + std::to_string(hr));
            return;
        }
        
        hr = m_videoDevice->CreateVideoProcessor(m_processorEnumerator.Get(), 0, &m_videoProcessor);
        if (FAILED(hr)) {
            Logger::Error("Failed to create video processor: " + std::to_string(hr));
            return;
        }
        
        // Set stream frame format to progressive
        m_videoContext->VideoProcessorSetStreamFrameFormat(m_videoProcessor.Get(), 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
        
        // Set source and destination rectangles to handle padding
        RECT sourceRect = { 0, 0, static_cast<LONG>(contentWidth), static_cast<LONG>(contentHeight) };
        m_videoContext->VideoProcessorSetStreamSourceRect(m_videoProcessor.Get(), 0, TRUE, &sourceRect);
        
        // Destination rect should also match content size (scaling happens in vertex shader)
        RECT destRect = { 0, 0, static_cast<LONG>(contentWidth), static_cast<LONG>(contentHeight) };
        m_videoContext->VideoProcessorSetStreamDestRect(m_videoProcessor.Get(), 0, TRUE, &destRect);
        
        // Create output view (reusable for same dimensions)
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
        outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputViewDesc.Texture2D.MipSlice = 0;
        
        hr = m_videoDevice->CreateVideoProcessorOutputView(m_procRgbaTexture.Get(), 
                                                            m_processorEnumerator.Get(),
                                                            &outputViewDesc, &m_outputView);
        if (FAILED(hr)) {
            Logger::Error("Failed to create output view: " + std::to_string(hr));
            return;
        }
        
        Logger::Info("Video processing resources created successfully");
    }
    
    // Store reference to our RGBA texture for rendering
    m_videoTexture = m_procRgbaTexture;
    m_videoSRV = m_procRgbaSRV;
    
    // Guard array slice index against texture dimensions to prevent driver buffer overrun
    if (arrayIndex < 0 || static_cast<UINT>(arrayIndex) >= texDesc.ArraySize) {
        arrayIndex = 0;
    }

    // Invalidate cached views if hardware texture source pointer changed
    if (m_lastSourceTexture != texture) {
        m_inputViews.clear();
        m_lastSourceTexture = texture;
    }

    // Now convert the NV12 frame to BGRA
    if (m_videoProcessor && m_videoDevice && m_videoContext && m_outputView) {
        UINT slice = static_cast<UINT>(arrayIndex);
        ComPtr<ID3D11VideoProcessorInputView> inputView;

        auto it = m_inputViews.find(slice);
        if (it != m_inputViews.end()) {
            inputView = it->second;
        } else {
            D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
            inputViewDesc.FourCC = 0;
            inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
            inputViewDesc.Texture2D.MipSlice = 0;
            inputViewDesc.Texture2D.ArraySlice = slice;

            HRESULT hr = m_videoDevice->CreateVideoProcessorInputView(texture, m_processorEnumerator.Get(), 
                                                                       &inputViewDesc, &inputView);
            if (SUCCEEDED(hr) && inputView) {
                m_inputViews[slice] = inputView;
            }
        }
        
        if (inputView) {
            bool bltSuccess = SafeVideoProcessorBlt(
                m_videoContext.Get(),
                m_videoProcessor.Get(),
                m_outputView.Get(),
                inputView.Get()
            );
            
            if (!bltSuccess) {
                static bool errorLogged = false;
                if (!errorLogged) {
                    Logger::Warning("VideoProcessorBlt encountered a driver issue, handled safely");
                    errorLogged = true;
                }
            }
        }
    }
}

ID3D11Device* RendererContext::GetDevice() {
    return DX11Device::GetInstance().GetDevice();
}

void RendererContext::SetVirtualDesktopBounds(const RECT& bounds) {
    m_virtualBounds = bounds;
    if (m_scalingMode == 5 && m_videoWidth > 0 && m_videoHeight > 0) {
        UpdateVertexBuffer();
    }
}

void RendererContext::SetScalingMode(int mode) {
    m_scalingMode = mode;
    if (m_videoWidth > 0 && m_videoHeight > 0) {
        UpdateVertexBuffer();
    }
}

void RendererContext::UpdateVertexBuffer() {
    if (!m_initialized || m_videoWidth <= 0 || m_videoHeight <= 0 || m_width <= 0 || m_height <= 0) {
        return;
    }

    float monitorAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    float videoAspect = static_cast<float>(m_videoWidth) / static_cast<float>(m_videoHeight);

    float quadLeft = -1.0f, quadRight = 1.0f;
    float quadTop = 1.0f, quadBottom = -1.0f;
    float uvLeft = 0.0f, uvRight = 1.0f;
    float uvTop = 0.0f, uvBottom = 1.0f;

    switch (m_scalingMode) {
        case 0: { // 0: Fill (Crop) - Fills the screen while maintaining aspect ratio; crops excess portions.
            if (videoAspect > monitorAspect) {
                // Video is wider than monitor - fit height, crop sides
                float scale = videoAspect / monitorAspect;
                quadLeft = -scale;
                quadRight = scale;
                quadTop = 1.0f;
                quadBottom = -1.0f;
            } else {
                // Video is taller than monitor - fit width, crop top/bottom
                float scale = monitorAspect / videoAspect;
                quadLeft = -1.0f;
                quadRight = 1.0f;
                quadTop = scale;
                quadBottom = -scale;
            }
            break;
        }

        case 1: { // 1: Fit (Contain) - Displays the entire image without cropping; leaves empty space if needed.
            if (videoAspect > monitorAspect) {
                // Video is wider than monitor - fit width, letterbox top/bottom
                float scale = monitorAspect / videoAspect;
                quadLeft = -1.0f;
                quadRight = 1.0f;
                quadTop = scale;
                quadBottom = -scale;
            } else {
                // Video is taller than monitor - fit height, pillarbox sides
                float scale = videoAspect / monitorAspect;
                quadLeft = -scale;
                quadRight = scale;
                quadTop = 1.0f;
                quadBottom = -1.0f;
            }
            break;
        }

        case 2: { // 2: Stretch - Stretches the image to match screen dimensions, ignoring aspect ratio.
            quadLeft = -1.0f;
            quadRight = 1.0f;
            quadTop = 1.0f;
            quadBottom = -1.0f;
            uvLeft = 0.0f;
            uvRight = 1.0f;
            uvTop = 0.0f;
            uvBottom = 1.0f;
            break;
        }

        case 3: { // 3: Center - Displays the image at its original resolution, centered on the screen.
            float scaleX = static_cast<float>(m_videoWidth) / static_cast<float>(m_width);
            float scaleY = static_cast<float>(m_videoHeight) / static_cast<float>(m_height);
            quadLeft = -scaleX;
            quadRight = scaleX;
            quadTop = scaleY;
            quadBottom = -scaleY;
            uvLeft = 0.0f;
            uvRight = 1.0f;
            uvTop = 0.0f;
            uvBottom = 1.0f;
            break;
        }

        case 4: { // 4: Tile - Repeats the image to cover the entire screen.
            quadLeft = -1.0f;
            quadRight = 1.0f;
            quadTop = 1.0f;
            quadBottom = -1.0f;
            uvLeft = 0.0f;
            uvRight = static_cast<float>(m_width) / static_cast<float>(m_videoWidth);
            uvTop = 0.0f;
            uvBottom = static_cast<float>(m_height) / static_cast<float>(m_videoHeight);
            break;
        }

        case 5: { // 5: Span - Spreads one wallpaper continuously across multiple monitors.
            quadLeft = -1.0f;
            quadRight = 1.0f;
            quadTop = 1.0f;
            quadBottom = -1.0f;

            int virtLeft = m_virtualBounds.left;
            int virtTop = m_virtualBounds.top;
            int virtWidth = m_virtualBounds.right - m_virtualBounds.left;
            int virtHeight = m_virtualBounds.bottom - m_virtualBounds.top;

            // If virtual desktop metrics not set, query system metrics
            if (virtWidth <= 0 || virtHeight <= 0) {
                virtLeft = GetSystemMetrics(SM_XVIRTUALSCREEN);
                virtTop = GetSystemMetrics(SM_YVIRTUALSCREEN);
                virtWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
                virtHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
            }

            if (virtWidth <= 0 || virtHeight <= 0) {
                // Fallback to single monitor Fill if virtual metrics unavailable
                if (videoAspect > monitorAspect) {
                    float scale = videoAspect / monitorAspect;
                    quadLeft = -scale;
                    quadRight = scale;
                } else {
                    float scale = monitorAspect / videoAspect;
                    quadTop = scale;
                    quadBottom = -scale;
                }
                break;
            }

            float virtAspect = static_cast<float>(virtWidth) / static_cast<float>(virtHeight);

            // Compute UV bounds across virtual canvas (Fill behavior across the multi-monitor span)
            float uMin = 0.0f, uMax = 1.0f;
            float vMin = 0.0f, vMax = 1.0f;

            if (videoAspect > virtAspect) {
                // Media is wider than the combined virtual canvas - fit height, crop excess sides
                float crop = (1.0f - (virtAspect / videoAspect)) * 0.5f;
                uMin = crop;
                uMax = 1.0f - crop;
            } else {
                // Media is taller than the combined virtual canvas - fit width, crop excess top/bottom
                float crop = (1.0f - (videoAspect / virtAspect)) * 0.5f;
                vMin = crop;
                vMax = 1.0f - crop;
            }

            // Determine monitor rect in virtual desktop space
            int monLeft = m_monitorBounds.left;
            int monRight = m_monitorBounds.right;
            int monTop = m_monitorBounds.top;
            int monBottom = m_monitorBounds.bottom;

            if (monRight <= monLeft || monBottom <= monTop) {
                monLeft = virtLeft;
                monRight = virtLeft + m_width;
                monTop = virtTop;
                monBottom = virtTop + m_height;
            }

            float tLeft = static_cast<float>(monLeft - virtLeft) / static_cast<float>(virtWidth);
            float tRight = static_cast<float>(monRight - virtLeft) / static_cast<float>(virtWidth);
            float tTop = static_cast<float>(monTop - virtTop) / static_cast<float>(virtHeight);
            float tBottom = static_cast<float>(monBottom - virtTop) / static_cast<float>(virtHeight);

            tLeft = std::clamp(tLeft, 0.0f, 1.0f);
            tRight = std::clamp(tRight, 0.0f, 1.0f);
            tTop = std::clamp(tTop, 0.0f, 1.0f);
            tBottom = std::clamp(tBottom, 0.0f, 1.0f);

            uvLeft = uMin + tLeft * (uMax - uMin);
            uvRight = uMin + tRight * (uMax - uMin);
            uvTop = vMin + tTop * (vMax - vMin);
            uvBottom = vMin + tBottom * (vMax - vMin);
            break;
        }

        default:
            break;
    }

    // Create vertices with calculated positions and UV coordinates
    Vertex vertices[] = {
        { { quadLeft,  quadTop, 0.0f },    { uvLeft,  uvTop } },     // Top-left
        { { quadRight, quadTop, 0.0f },    { uvRight, uvTop } },     // Top-right
        { { quadLeft,  quadBottom, 0.0f }, { uvLeft,  uvBottom } },  // Bottom-left
        { { quadRight, quadBottom, 0.0f }, { uvRight, uvBottom } },  // Bottom-right
    };

    // Update vertex buffer
    auto* context = DX11Device::GetInstance().GetContext();
    if (context && m_vertexBuffer) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        HRESULT hr = context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(mapped.pData, vertices, sizeof(vertices));
            context->Unmap(m_vertexBuffer.Get(), 0);
        }
    }
}

} // namespace PixelMotion
