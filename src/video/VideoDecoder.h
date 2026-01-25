#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <string>
#include <memory>

// Forward declarations for FFmpeg types
struct AVFormatContext;
struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct AVBufferRef;

namespace PixelMotion {

/**
 * FFmpeg-based video decoder with D3D11VA hardware acceleration
 */
class VideoDecoder {
public:
    VideoDecoder();
    ~VideoDecoder();

    bool Initialize(const std::wstring& filePath, ID3D11Device* device);
    void Shutdown();

    bool DecodeNextFrame();
    ID3D11Texture2D* GetFrameTexture();
    /**
     * Get texture array index for D3D11VA frames
     */
    int GetFrameArrayIndex();
    
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    double GetDuration() const { return m_duration; }
    double GetFrameRate() const { return m_frameRate; }
    bool IsEndOfFile() const { return m_eof; }
    bool IsImage() const { return m_isImage; }
    
    void Seek(double timeSeconds);
    void Reset(); // Seek to beginning

private:
    bool OpenFile(const std::wstring& filePath);
    bool FindVideoStream();
    bool InitializeDecoder(ID3D11Device* device);
    bool SetupHardwareAcceleration(ID3D11Device* device);

    AVFormatContext* m_formatContext;
    AVCodecContext* m_codecContext;
    AVFrame* m_frame;
    AVPacket* m_packet;
    AVBufferRef* m_hwDeviceCtx;

    int m_width;
    int m_height;
    double m_duration;
    double m_frameRate;
    int m_videoStreamIndex;
    bool m_eof;
    bool m_initialized;
    bool m_isImage;
};

} // namespace PixelMotion
