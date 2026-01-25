#include "DX11Device.h"
#include "core/Logger.h"

#include <dxgi1_2.h>

namespace PixelMotion {

DX11Device& DX11Device::GetInstance() {
    static DX11Device instance;
    return instance;
}

bool DX11Device::Initialize() {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing DirectX 11 device...");

    UINT createDeviceFlags = 0;
#ifdef DEBUG_BUILD
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // Adapter (nullptr = default)
        D3D_DRIVER_TYPE_HARDWARE,   // Driver type
        nullptr,                    // Software rasterizer
        createDeviceFlags,          // Flags
        featureLevels,              // Feature levels
        ARRAYSIZE(featureLevels),   // Num feature levels
        D3D11_SDK_VERSION,          // SDK version
        &m_device,                  // Device output
        &m_featureLevel,            // Feature level output
        &m_context                  // Context output
    );

    if (FAILED(hr)) {
        Logger::Error("Failed to create D3D11 device");
        return false;
    }

    // Get DXGI factory
    ComPtr<IDXGIDevice> dxgiDevice;
    hr = m_device.As(&dxgiDevice);
    if (FAILED(hr)) {
        Logger::Error("Failed to get DXGI device");
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    if (FAILED(hr)) {
        Logger::Error("Failed to get DXGI adapter");
        return false;
    }

    hr = adapter->GetParent(__uuidof(IDXGIFactory2), &m_factory);
    if (FAILED(hr)) {
        Logger::Error("Failed to get DXGI factory");
        return false;
    }

    m_initialized = true;
    Logger::Info("DirectX 11 device initialized successfully");
    return true;
}

void DX11Device::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down DirectX 11 device...");

    m_factory.Reset();
    m_context.Reset();
    m_device.Reset();

    m_initialized = false;
}

} // namespace PixelMotion
