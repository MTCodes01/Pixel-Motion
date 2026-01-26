void RendererContext::SetScalingMode(int mode) {
    if (m_scalingMode != mode) {
        m_scalingMode = mode;
        if (m_videoWidth > 0 && m_videoHeight > 0) {
            UpdateVertexBuffer();
        }
    }
}

void RendererContext::UpdateVertexBuffer() {
    if (!m_initialized || m_videoWidth == 0 || m_videoHeight == 0) {
        return;
    }

    float monitorAspect = static_cast<float>(m_width) / static_cast<float>(m_height);
    float videoAspect = static_cast<float>(m_videoWidth) / static_cast<float>(m_videoHeight);

    float quadLeft = -1.0f, quadRight = 1.0f;
    float quadTop = 1.0f, quadBottom = -1.0f;

    switch (m_scalingMode) {
        case 0: { // Fill - scale to cover, crop if needed
            if (videoAspect > monitorAspect) {
                // Video is wider - fit height, crop sides
                float scale = monitorAspect / videoAspect;
                quadLeft = -1.0f / scale;
                quadRight = 1.0f / scale;
            } else {
                // Video is taller - fit width, crop top/bottom
                float scale = videoAspect / monitorAspect;
                quadTop = 1.0f / scale;
                quadBottom = -1.0f / scale;
            }
            break;
        }

        case 1: { // Fit - scale to fit inside, letterbox if needed
            if (videoAspect > monitorAspect) {
                // Video is wider - fit width, add bars top/bottom
                float scale = videoAspect / monitorAspect;
                quadTop = 1.0f * scale;
                quadBottom = -1.0f * scale;
            } else {
                // Video is taller - fit height, add bars on sides
                float scale = monitorAspect / videoAspect;
                quadLeft = -1.0f * scale;
                quadRight = 1.0f * scale;
            }
            break;
        }

        case 2: { // Stretch - fill screen (ignore aspect ratio)
            // Use default fullscreen quad
            break;
        }

        case 3: { // Center - original size, centered
            float scaleX = static_cast<float>(m_videoWidth) / static_cast<float>(m_width);
            float scaleY = static_cast<float>(m_videoHeight) / static_cast<float>(m_height);
            quadLeft = -scaleX;
            quadRight = scaleX;
            quadTop = scaleY;
            quadBottom = -scaleY;
            break;
        }
    }

    // Create vertices with calculated positions
    Vertex vertices[] = {
        { { quadLeft,  quadTop, 0.0f }, { 0.0f, 0.0f } },     // Top-left
        { { quadRight, quadTop, 0.0f }, { 1.0f, 0.0f } },     // Top-right
        { { quadLeft,  quadBottom, 0.0f }, { 0.0f, 1.0f } },  // Bottom-left
        { { quadRight, quadBottom, 0.0f }, { 1.0f, 1.0f } },  // Bottom-right
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
