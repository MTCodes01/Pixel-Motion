#include "VideoDecoder.h"
#include "core/Logger.h"

#include <codecvt>
#include <locale>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
#include <libavutil/imgutils.h>
}

namespace PixelMotion {

VideoDecoder::VideoDecoder()
    : m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
    , m_hwDeviceCtx(nullptr)
    , m_width(0)
    , m_height(0)
    , m_duration(0.0)
    , m_frameRate(0.0)
    , m_videoStreamIndex(-1)
    , m_eof(false)
    , m_initialized(false)
    , m_isImage(false)
{
}

VideoDecoder::~VideoDecoder() {
    Shutdown();
}

bool VideoDecoder::Initialize(const std::wstring& filePath, ID3D11Device* device) {
    if (m_initialized) {
        return true;
    }

    Logger::Info("Initializing video decoder...");

    if (!OpenFile(filePath)) {
        Logger::Error("Failed to open video file");
        return false;
    }

    if (!FindVideoStream()) {
        Logger::Error("Failed to find video stream");
        Shutdown();
        return false;
    }

    if (!InitializeDecoder(device)) {
        Logger::Error("Failed to initialize decoder");
        Shutdown();
        return false;
    }

    m_initialized = true;
    Logger::Info("Video decoder initialized successfully");
    return true;
}

void VideoDecoder::Shutdown() {
    if (!m_initialized) {
        return;
    }

    Logger::Info("Shutting down video decoder...");

    if (m_packet) {
        av_packet_free(&m_packet);
    }

    if (m_frame) {
        av_frame_free(&m_frame);
    }

    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }

    if (m_formatContext) {
        avformat_close_input(&m_formatContext);
    }

    if (m_hwDeviceCtx) {
        av_buffer_unref(&m_hwDeviceCtx);
    }

    m_initialized = false;
}

bool VideoDecoder::OpenFile(const std::wstring& filePath) {
    // Convert wstring to UTF-8 string
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    std::string utf8Path = converter.to_bytes(filePath);

    // Open video file
    if (avformat_open_input(&m_formatContext, utf8Path.c_str(), nullptr, nullptr) < 0) {
        Logger::Error("Could not open video file: " + utf8Path);
        return false;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(m_formatContext, nullptr) < 0) {
        Logger::Error("Could not find stream information");
        return false;
    }

    // Get duration
    if (m_formatContext->duration != AV_NOPTS_VALUE) {
        m_duration = static_cast<double>(m_formatContext->duration) / AV_TIME_BASE;
    }

    Logger::Info("Opened video file: " + utf8Path);
    Logger::Info("Duration: " + std::to_string(m_duration) + " seconds");

    return true;
}

bool VideoDecoder::FindVideoStream() {
    m_videoStreamIndex = -1;

    for (unsigned int i = 0; i < m_formatContext->nb_streams; i++) {
        if (m_formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            m_videoStreamIndex = i;
            break;
        }
    }

    if (m_videoStreamIndex == -1) {
        Logger::Error("Could not find video stream");
        return false;
    }

    AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
    
    // Find decoder to get codec ID for image detection
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        Logger::Error("Unsupported codec for video stream");
        return false;
    }

    m_width = videoStream->codecpar->width;
    m_height = videoStream->codecpar->height;
    m_frameRate = av_q2d(videoStream->avg_frame_rate);
    
    // Detect if this is an image file (single frame)
    // Image codecs: MJPEG, PNG, BMP, etc.
    if (codec->id == AV_CODEC_ID_MJPEG || codec->id == AV_CODEC_ID_PNG || 
        codec->id == AV_CODEC_ID_BMP || codec->id == AV_CODEC_ID_TIFF ||
        codec->id == AV_CODEC_ID_WEBP || codec->id == AV_CODEC_ID_JPEG2000 ||
        videoStream->nb_frames == 1 || m_duration < 0.1) {
        m_isImage = true;
        Logger::Info("Detected image file: " + std::to_string(m_width) + "x" + std::to_string(m_height));
    } else {
        Logger::Info("Video stream found: " + std::to_string(m_width) + "x" + 
                     std::to_string(m_height) + " @ " + std::to_string(m_frameRate) + " fps");
    }

    return true;
}

bool VideoDecoder::InitializeDecoder(ID3D11Device* device) {
    AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
    
    // Find decoder
    const AVCodec* codec = avcodec_find_decoder(videoStream->codecpar->codec_id);
    if (!codec) {
        Logger::Error("Unsupported codec");
        return false;
    }

    // Allocate codec context
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        Logger::Error("Could not allocate codec context");
        return false;
    }

    // Copy codec parameters
    if (avcodec_parameters_to_context(m_codecContext, videoStream->codecpar) < 0) {
        Logger::Error("Could not copy codec parameters");
        return false;
    }

    // Setup hardware acceleration
    if (device && !SetupHardwareAcceleration(device)) {
        Logger::Warning("Hardware acceleration setup failed, falling back to software decoding");
    }

    // Set pixel format callback to prefer hardware formats
    m_codecContext->get_format = [](AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts) -> enum AVPixelFormat {
        const enum AVPixelFormat* p;
        for (p = pix_fmts; *p != AV_PIX_FMT_NONE; p++) {
            if (*p == AV_PIX_FMT_D3D11) {
                return *p;
            }
        }
        // Fallback to first available format
        return pix_fmts[0];
    };

    // Open codec
    if (avcodec_open2(m_codecContext, codec, nullptr) < 0) {
        Logger::Error("Could not open codec");
        return false;
    }

    // Allocate frame and packet
    m_frame = av_frame_alloc();
    m_packet = av_packet_alloc();

    if (!m_frame || !m_packet) {
        Logger::Error("Could not allocate frame or packet");
        return false;
    }

    Logger::Info("Decoder initialized successfully");
    return true;
}

bool VideoDecoder::SetupHardwareAcceleration(ID3D11Device* device) {
    // Create D3D11VA device context
    AVBufferRef* hwDeviceCtx = av_hwdevice_ctx_alloc(AV_HWDEVICE_TYPE_D3D11VA);
    if (!hwDeviceCtx) {
        Logger::Warning("Could not allocate D3D11VA device context");
        return false;
    }

    AVHWDeviceContext* deviceCtx = reinterpret_cast<AVHWDeviceContext*>(hwDeviceCtx->data);
    AVD3D11VADeviceContext* d3d11vaCtx = reinterpret_cast<AVD3D11VADeviceContext*>(deviceCtx->hwctx);

    // Use our existing D3D11 device
    d3d11vaCtx->device = device;
    device->AddRef(); // FFmpeg will release it

    // Initialize hardware device context
    if (av_hwdevice_ctx_init(hwDeviceCtx) < 0) {
        Logger::Warning("Could not initialize D3D11VA device context");
        av_buffer_unref(&hwDeviceCtx);
        return false;
    }

    // Set hardware device context in codec
    m_codecContext->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
    m_hwDeviceCtx = hwDeviceCtx;

    Logger::Info("D3D11VA hardware acceleration enabled");
    return true;
}

bool VideoDecoder::DecodeNextFrame() {
    if (!m_initialized) {
        return false;
    }

    while (true) {
        // Read packet
        int ret = av_read_frame(m_formatContext, m_packet);
        
        if (ret < 0) {
            if (ret == AVERROR_EOF) {
                m_eof = true;
                Logger::Info("End of video file reached");
            } else {
                Logger::Error("Error reading frame");
            }
            return false;
        }

        // Skip non-video packets
        if (m_packet->stream_index != m_videoStreamIndex) {
            av_packet_unref(m_packet);
            continue;
        }

        // Send packet to decoder
        ret = avcodec_send_packet(m_codecContext, m_packet);
        av_packet_unref(m_packet);

        if (ret < 0) {
            Logger::Error("Error sending packet to decoder");
            return false;
        }

        // Receive decoded frame
        ret = avcodec_receive_frame(m_codecContext, m_frame);
        
        if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            continue;
        } else if (ret < 0) {
            Logger::Error("Error receiving frame from decoder");
            return false;
        }

        // Successfully decoded a frame
        return true;
    }
}

ID3D11Texture2D* VideoDecoder::GetFrameTexture() {
    if (!m_frame || !m_frame->data[0]) {
        return nullptr;
    }

    // If hardware decoding, frame->data[0] contains ID3D11Texture2D*
    if (m_frame->format == AV_PIX_FMT_D3D11) {
        return reinterpret_cast<ID3D11Texture2D*>(m_frame->data[0]);
    }

    // Software decoding - need to upload frame data to GPU texture
    // This is a fallback when hardware acceleration isn't available
    
    // For software frames, we need to:
    // 1. Create a staging texture if we don't have one
    // 2. Copy frame data to staging texture
    // 3. Return the texture
    
    // Note: This is a simplified implementation
    // A full implementation would handle format conversion (YUV->RGB)
    // For now, we'll just log and return nullptr
    // The proper solution is to ensure hardware decoding works
    
    static bool warningShown = false;
    if (!warningShown) {
        Logger::Warning("Software decoded frame - texture upload not fully implemented");
        Logger::Warning("Please ensure your GPU supports D3D11VA for this video codec");
        warningShown = true;
    }
    
    return nullptr;
}

int VideoDecoder::GetFrameArrayIndex() {
    if (!m_frame || m_frame->format != AV_PIX_FMT_D3D11) {
        return 0;
    }
    
    // For D3D11VA, frame->data[1] contains the array slice index as intptr_t
    return static_cast<int>(reinterpret_cast<intptr_t>(m_frame->data[1]));
}

void VideoDecoder::Seek(double timeSeconds) {
    if (!m_initialized) {
        return;
    }

    int64_t timestamp = static_cast<int64_t>(timeSeconds * AV_TIME_BASE);
    
    if (av_seek_frame(m_formatContext, -1, timestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        Logger::Error("Seek failed");
        return;
    }

    avcodec_flush_buffers(m_codecContext);
    m_eof = false;
}

void VideoDecoder::Reset() {
    Seek(0.0);
}

} // namespace PixelMotion
