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
#include <libswscale/swscale.h>
}

namespace PixelMotion {

VideoDecoder::VideoDecoder()
    : m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
    , m_hwDeviceCtx(nullptr)
    , m_swsContext(nullptr)
    , m_rgbaFrame(nullptr)
    , m_device(nullptr)
    , m_textureUploaded(false)
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

    if (m_rgbaFrame) {
        av_frame_free(&m_rgbaFrame);
    }

    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
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

    m_softwareTexture.Reset();
    m_device = nullptr;
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
    
    // Get basic video info
    m_width = videoStream->codecpar->width;
    m_height = videoStream->codecpar->height;
    m_frameRate = av_q2d(videoStream->avg_frame_rate);
    
    // Detect if this is an image file based on codec ID
    // Image codecs: MJPEG, PNG, BMP, etc.
    AVCodecID codecId = videoStream->codecpar->codec_id;
    if (codecId == AV_CODEC_ID_MJPEG || codecId == AV_CODEC_ID_PNG || 
        codecId == AV_CODEC_ID_BMP || codecId == AV_CODEC_ID_TIFF ||
        codecId == AV_CODEC_ID_WEBP || codecId == AV_CODEC_ID_JPEG2000 ||
        codecId == AV_CODEC_ID_GIF ||
        videoStream->nb_frames == 1 || m_duration < 0.1) {
        m_isImage = true;
        
        // For some image formats, dimensions might be 0 until we decode
        // We'll get the actual dimensions after decoding the first frame
        if (m_width == 0 || m_height == 0) {
            Logger::Info("Image dimensions not in header, will get from decoded frame");
        } else {
            Logger::Info("Detected image file: " + std::to_string(m_width) + "x" + std::to_string(m_height));
        }
    } else {
        Logger::Info("Video stream found: " + std::to_string(m_width) + "x" + 
                     std::to_string(m_height) + " @ " + std::to_string(m_frameRate) + " fps");
    }

    return true;
}

bool VideoDecoder::InitializeDecoder(ID3D11Device* device) {
    AVStream* videoStream = m_formatContext->streams[m_videoStreamIndex];
    
    // Find decoder
    AVCodecID codecId = videoStream->codecpar->codec_id;
    Logger::Info("Looking for decoder for codec ID: " + std::to_string(static_cast<int>(codecId)));
    
    const AVCodec* codec = avcodec_find_decoder(codecId);
    if (!codec) {
        Logger::Error("Unsupported codec ID: " + std::to_string(static_cast<int>(codecId)));
        return false;
    }
    
    Logger::Info("Found decoder: " + std::string(codec->name));

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

    // For images, skip hardware acceleration (not supported)
    // For videos, try to use hardware acceleration
    if (!m_isImage && device) {
        if (!SetupHardwareAcceleration(device)) {
            Logger::Warning("Hardware acceleration setup failed, falling back to software decoding");
        }
        
        // Set pixel format callback to prefer hardware formats for videos
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
    } else if (m_isImage) {
        Logger::Info("Image file - using software decoding");
    }

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

    // Store device for software texture upload
    m_device = device;

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
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            Logger::Error("Error sending packet to decoder: " + std::string(errbuf) + " (" + std::to_string(ret) + ")");
            return false;
        }

        // Receive decoded frame
        ret = avcodec_receive_frame(m_codecContext, m_frame);
        
        if (ret == AVERROR(EAGAIN)) {
            // Need more packets
            continue;
        } else if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
            Logger::Error("Error receiving frame from decoder: " + std::string(errbuf));
            return false;
        }

        // Successfully decoded a frame
        // For images with 0x0 dimensions, get actual size from decoded frame
        if (m_isImage && (m_width == 0 || m_height == 0)) {
            m_width = m_frame->width;
            m_height = m_frame->height;
            Logger::Info("Got image dimensions from frame: " + std::to_string(m_width) + "x" + std::to_string(m_height));
        }
        
        m_textureUploaded = false; // New frame needs upload
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

    // Software decoding - need to convert and upload frame data to GPU texture
    if (!m_device) {
        static bool warned = false;
        if (!warned) {
            Logger::Warning("No D3D11 device available for software frame upload");
            warned = true;
        }
        return nullptr;
    }

    // Create or update software texture if needed
    if (!m_softwareTexture) {
        D3D11_TEXTURE2D_DESC texDesc = {};
        texDesc.Width = m_frame->width;
        texDesc.Height = m_frame->height;
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_DYNAMIC;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = m_device->CreateTexture2D(&texDesc, nullptr, &m_softwareTexture);
        if (FAILED(hr)) {
            Logger::Error("Failed to create software texture: " + std::to_string(hr));
            return nullptr;
        }
        Logger::Info("Created software upload texture: " + std::to_string(m_frame->width) + "x" + std::to_string(m_frame->height));
        m_textureUploaded = false; // Force upload for new texture
    }

    // If texture is already uploaded, just return it (optimization for static images)
    if (m_textureUploaded) {
        return m_softwareTexture.Get();
    }

    // Set up swscale context if needed (convert any format to BGRA)
    if (!m_swsContext) {
        m_swsContext = sws_getContext(
            m_frame->width, m_frame->height, (AVPixelFormat)m_frame->format,
            m_frame->width, m_frame->height, AV_PIX_FMT_BGRA,
            SWS_BILINEAR, nullptr, nullptr, nullptr
        );
        if (!m_swsContext) {
            Logger::Error("Failed to create swscale context");
            return nullptr;
        }
        Logger::Info("Created swscale context for format " + std::to_string(m_frame->format) + " -> BGRA");
    }

    // Map the texture for writing
    ID3D11DeviceContext* context = nullptr;
    m_device->GetImmediateContext(&context);
    if (!context) {
        return nullptr;
    }

    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr = context->Map(m_softwareTexture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) {
        context->Release();
        Logger::Error("Failed to map texture: " + std::to_string(hr));
        return nullptr;
    }

    // Convert frame to BGRA and write directly to mapped texture
    uint8_t* dstData[1] = { (uint8_t*)mapped.pData };
    int dstLinesize[1] = { (int)mapped.RowPitch };

    sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 
              0, m_frame->height, dstData, dstLinesize);

    context->Unmap(m_softwareTexture.Get(), 0);
    context->Release();

    m_textureUploaded = true;
    return m_softwareTexture.Get();
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
