#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace PixelMotion {

/**
 * DirectX 11 device manager
 * Shared D3D11 device and context for all renderers
 */
class DX11Device {
public:
    static DX11Device& GetInstance();

    bool Initialize();
    void Shutdown();

    ID3D11Device* GetDevice() const { return m_device.Get(); }
    ID3D11DeviceContext* GetContext() const { return m_context.Get(); }
    IDXGIFactory2* GetFactory() const { return m_factory.Get(); }

private:
    DX11Device() = default;
    ~DX11Device() = default;

    ComPtr<ID3D11Device> m_device;
    ComPtr<ID3D11DeviceContext> m_context;
    ComPtr<IDXGIFactory2> m_factory;
    D3D_FEATURE_LEVEL m_featureLevel;

    bool m_initialized = false;
};

} // namespace PixelMotion
