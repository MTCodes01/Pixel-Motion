#include "RendererContext.h"
#include "DX11Device.h"
#include "core/Logger.h"

#include <d3dcompiler.h>
#include <d3d11_1.h>
#include <filesystem>

namespace PixelMotion {

// Simple vertex structure
struct Vertex {
    float position[3];
    float texCoord[2];
};

RendererContext::RendererContext()
    : m_hwnd(nullptr)
    , m_width(0)
    , m_height(0)
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

    // Create sampler state
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

    m_initialized = true;
    Logger::Info("Renderer context initialized successfully");
    return true;
}

void RendererContext::Shutdown() {
    if (!m_initialized) {
        return;
    }

    m_samplerState.Reset();
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
    // Fullscreen quad vertices
    Vertex vertices[] = {
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 0.0f } },  // Top-left
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f } },  // Top-right
        { { -1.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },  // Bottom-left
        { {  1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } },  // Bottom-right
    };

    D3D11_BUFFER_DESC bufferDesc = {};
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.ByteWidth = sizeof(vertices);
    bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

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
        context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

        context->Draw(4, 0);
    }
}

void RendererContext::Present() {
    if (m_swapChain) {
        m_swapChain->Present(1, 0); // VSync
    }
}

void RendererContext::SetVideoTexture(ID3D11Texture2D* texture, int arrayIndex) {
    if (!texture) {
        m_videoSRV.Reset();
        return;
    }

    // Get texture description
    D3D11_TEXTURE2D_DESC texDesc;
    texture->GetDesc(&texDesc);

    auto* device = DX11Device::GetInstance().GetDevice();
    auto* context = DX11Device::GetInstance().GetContext();

    // Check format - if BGRA/RGBA, use directly
    if (texDesc.Format == DXGI_FORMAT_B8G8R8A8_UNORM || 
        texDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM) {
        
        m_videoTexture = texture;
        
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

    // Static variables to cache video processing resources
    static ComPtr<ID3D11Texture2D> s_rgbaTexture;
    static ComPtr<ID3D11ShaderResourceView> s_rgbaSRV;
    static ComPtr<ID3D11VideoDevice> s_videoDevice;
    static ComPtr<ID3D11VideoContext> s_videoContext;
    static ComPtr<ID3D11VideoProcessor> s_videoProcessor;
    static ComPtr<ID3D11VideoProcessorEnumerator> s_processorEnumerator;
    static ComPtr<ID3D11VideoProcessorOutputView> s_outputView;
    static UINT s_videoWidth = 0;
    static UINT s_videoHeight = 0;
    
    // Check if we need to create/recreate resources for new video dimensions
    if (texDesc.Width != s_videoWidth || texDesc.Height != s_videoHeight) {
        s_videoWidth = texDesc.Width;
        s_videoHeight = texDesc.Height;
        
        Logger::Info("Creating video processing resources for " + 
                     std::to_string(s_videoWidth) + "x" + std::to_string(s_videoHeight));
        
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
        
        HRESULT hr = device->CreateTexture2D(&rgbaDesc, nullptr, &s_rgbaTexture);
        if (FAILED(hr)) {
            Logger::Error("Failed to create RGBA texture: " + std::to_string(hr));
            return;
        }
        
        // Create SRV for the RGBA texture
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        
        hr = device->CreateShaderResourceView(s_rgbaTexture.Get(), &srvDesc, &s_rgbaSRV);
        if (FAILED(hr)) {
            Logger::Error("Failed to create SRV: " + std::to_string(hr));
            return;
        }
        
        // Get video device interface
        hr = device->QueryInterface(__uuidof(ID3D11VideoDevice), &s_videoDevice);
        if (FAILED(hr)) {
            Logger::Error("Failed to get ID3D11VideoDevice: " + std::to_string(hr));
            return;
        }
        
        hr = context->QueryInterface(__uuidof(ID3D11VideoContext), &s_videoContext);
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
        
        hr = s_videoDevice->CreateVideoProcessorEnumerator(&contentDesc, &s_processorEnumerator);
        if (FAILED(hr)) {
            Logger::Error("Failed to create video processor enumerator: " + std::to_string(hr));
            return;
        }
        
        hr = s_videoDevice->CreateVideoProcessor(s_processorEnumerator.Get(), 0, &s_videoProcessor);
        if (FAILED(hr)) {
            Logger::Error("Failed to create video processor: " + std::to_string(hr));
            return;
        }
        
        // Create output view (reusable for same dimensions)
        D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC outputViewDesc = {};
        outputViewDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
        outputViewDesc.Texture2D.MipSlice = 0;
        
        hr = s_videoDevice->CreateVideoProcessorOutputView(s_rgbaTexture.Get(), 
                                                            s_processorEnumerator.Get(),
                                                            &outputViewDesc, &s_outputView);
        if (FAILED(hr)) {
            Logger::Error("Failed to create output view: " + std::to_string(hr));
            return;
        }
        
        Logger::Info("Video processing resources created successfully");
    }
    
    // Store reference to our RGBA texture for rendering
    m_videoTexture = s_rgbaTexture;
    m_videoSRV = s_rgbaSRV;
    
    // Now convert the NV12 frame to BGRA
    if (s_videoProcessor && s_videoDevice && s_videoContext && s_outputView) {
        // Create input view for this frame - use the correct array slice!
        D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC inputViewDesc = {};
        inputViewDesc.FourCC = 0;
        inputViewDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
        inputViewDesc.Texture2D.MipSlice = 0;
        inputViewDesc.Texture2D.ArraySlice = arrayIndex;  // Use the frame's array index!
        
        ComPtr<ID3D11VideoProcessorInputView> inputView;
        HRESULT hr = s_videoDevice->CreateVideoProcessorInputView(texture, s_processorEnumerator.Get(), 
                                                                   &inputViewDesc, &inputView);
        
        if (SUCCEEDED(hr)) {
            // Perform the conversion
            D3D11_VIDEO_PROCESSOR_STREAM stream = {};
            stream.Enable = TRUE;
            stream.pInputSurface = inputView.Get();
            
            hr = s_videoContext->VideoProcessorBlt(s_videoProcessor.Get(), 
                                                    s_outputView.Get(), 
                                                    0, 1, &stream);
            
            if (FAILED(hr)) {
                static bool errorLogged = false;
                if (!errorLogged) {
                    Logger::Error("VideoProcessorBlt failed: " + std::to_string(hr));
                    errorLogged = true;
                }
            }
        } else {
            static bool inputViewError = false;
            if (!inputViewError) {
                Logger::Error("Failed to create input view: " + std::to_string(hr));
                inputViewError = true;
            }
        }
    }
}

ID3D11Device* RendererContext::GetDevice() {
    return DX11Device::GetInstance().GetDevice();
}

} // namespace PixelMotion
