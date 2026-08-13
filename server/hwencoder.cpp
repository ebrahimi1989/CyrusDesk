#include "hwencoder.h"
#include <QDebug>
#include <chrono>

HWEncoder::HWEncoder(QObject* parent)
    : QObject(parent)
    , m_codecContext(nullptr)
    , m_codec(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
    , m_swsContext(nullptr)
    , m_hwDeviceContext(nullptr)
    , m_hwType(Auto)
    , m_hwAccelEnabled(false)
    , m_width(0)
    , m_height(0)
    , m_fps(60)
    , m_bitrate(8000000)
    , m_frameCount(0)
    , m_running(false)
    , m_lastFrameCount(0)
    , m_initialized(false)
{
    memset(&m_stats, 0, sizeof(Stats));
}

HWEncoder::~HWEncoder()
{
    qDebug() << "HWEncoder destructor called";

    // Stop encoder thread
    m_running = false;

    // Wake up threads
    m_frameCV.notify_all();
    m_queueCV.notify_all();

    // Clear queues
    {
        std::lock_guard<std::mutex> lock1(m_frameMutex);
        std::lock_guard<std::mutex> lock2(m_queueMutex);
        while (!m_frameQueue.empty()) m_frameQueue.pop();
        while (!m_packetQueue.empty()) m_packetQueue.pop();
    }

    // Wait for thread to finish
    if (m_encoderThread && m_encoderThread->joinable()) {
        qDebug() << "Waiting for encoder thread to finish...";
        m_encoderThread->join();
        qDebug() << "Encoder thread finished";
    }

    // Flush encoder
    if (m_codecContext && m_initialized) {
        avcodec_send_frame(m_codecContext, nullptr); // Flush
    }

    // Free FFmpeg resources
    if (m_frame) {
        av_frame_free(&m_frame);
    }
    if (m_packet) {
        av_packet_free(&m_packet);
    }
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
    }
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
    }
    if (m_hwDeviceContext) {
        av_buffer_unref(&m_hwDeviceContext);
    }

    qDebug() << "HWEncoder cleanup complete";
}

bool HWEncoder::initialize(int width, int height, int fps, int bitrate, HWAccelType type)
{
    m_width = width;
    m_height = height;
    m_fps = fps;
    m_bitrate = bitrate;
    m_hwType = type;

    // Try hardware acceleration first
    if (type == Auto) {
        // Try NVENC first (fastest)
        if (initializeCodec(NVENC)) {
            qDebug() << "Initialized with NVENC";
            m_hwType = NVENC;
        }
        // Try VAAPI (good for Intel/AMD on Linux)
        else if (initializeCodec(VAAPI)) {
            qDebug() << "Initialized with VAAPI";
            m_hwType = VAAPI;
        }
        // Try QSV (Intel Quick Sync)
        else if (initializeCodec(QSV)) {
            qDebug() << "Initialized with QSV";
            m_hwType = QSV;
        }
        // Fallback to software
        else if (initializeCodec(Software)) {
            qDebug() << "Initialized with Software encoder";
            m_hwType = Software;
        }
        else {
            qCritical() << "Failed to initialize any encoder";
            return false;
        }
    } else {
        if (!initializeCodec(type)) {
            qCritical() << "Failed to initialize requested encoder";
            return false;
        }
    }

    // Start encoder thread
    m_running = true;
    m_encoderThread = std::make_unique<std::thread>(&HWEncoder::encoderThreadFunc, this);

    m_initialized = true;
    m_lastFrameTime = std::chrono::steady_clock::now();

    return true;
}

const char* HWEncoder::getEncoderName(HWAccelType type)
{
    switch (type) {
        case NVENC: return "h264_nvenc";
        case VAAPI: return "h264_vaapi";
        case QSV: return "h264_qsv";
        case AMF: return "h264_amf";
        case Software: return "libx264";
        default: return "libx264";
    }
}

AVHWDeviceType HWEncoder::getHWDeviceType(HWAccelType type)
{
    switch (type) {
        case NVENC: return AV_HWDEVICE_TYPE_CUDA;
        case VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
        case QSV: return AV_HWDEVICE_TYPE_QSV;
        case AMF: return AV_HWDEVICE_TYPE_D3D11VA;
        default: return AV_HWDEVICE_TYPE_NONE;
    }
}

bool HWEncoder::initializeHardwareAccel(AVHWDeviceType hwType)
{
    if (hwType == AV_HWDEVICE_TYPE_NONE) {
        return false;
    }

    int ret = av_hwdevice_ctx_create(&m_hwDeviceContext, hwType, nullptr, nullptr, 0);
    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "Failed to create HW device context:" << err;
        return false;
    }

    m_codecContext->hw_device_ctx = av_buffer_ref(m_hwDeviceContext);
    m_hwAccelEnabled = true;
    return true;
}

bool HWEncoder::initializeCodec(HWAccelType type)
{
    const char* encoderName = getEncoderName(type);
    m_codec = avcodec_find_encoder_by_name(encoderName);

    if (!m_codec) {
        qWarning() << "Encoder not found:" << encoderName;
        return false;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        qCritical() << "Could not allocate video codec context";
        return false;
    }

    // Balanced low-latency + quality configuration
    m_codecContext->width = m_width;
    m_codecContext->height = m_height;
    m_codecContext->time_base = AVRational{1, m_fps};
    m_codecContext->framerate = AVRational{m_fps, 1};
    m_codecContext->bit_rate = m_bitrate;
    m_codecContext->gop_size = 30; // Keyframe every 30 frames (~500ms at 60fps) - better quality
    m_codecContext->max_b_frames = 0; // No B-frames for lowest latency
    m_codecContext->pix_fmt = (type == Software) ? AV_PIX_FMT_YUV420P : AV_PIX_FMT_NV12;
    m_codecContext->thread_count = 2; // Reduced threads for lower latency
    m_codecContext->delay = 0; // Zero frame delay

    // Quality settings
    m_codecContext->qmin = 18; // Minimum quantizer (better quality)
    m_codecContext->qmax = 32; // Maximum quantizer (prevent too much compression)

    // Low latency flags
    m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;

    // Hardware acceleration setup
    if (type != Software) {
        AVHWDeviceType hwType = getHWDeviceType(type);
        if (!initializeHardwareAccel(hwType)) {
            avcodec_free_context(&m_codecContext);
            return false;
        }
    }

    // Additional low-latency options for NVENC
    if (type == NVENC) {
        av_opt_set(m_codecContext->priv_data, "delay", "0", 0);
        av_opt_set(m_codecContext->priv_data, "zerolatency", "1", 0);
        av_opt_set(m_codecContext->priv_data, "rc", "vbr", 0); // Variable bitrate for better quality
        av_opt_set(m_codecContext->priv_data, "cq", "23", 0); // Constant quality
        av_opt_set_int(m_codecContext->priv_data, "surfaces", 2, 0); // 2 surfaces for stability
        av_opt_set(m_codecContext->priv_data, "spatial-aq", "1", 0); // Spatial AQ for quality
    }

    // VAAPI specific settings
    if (type == VAAPI) {
        av_opt_set_int(m_codecContext->priv_data, "quality", 4, 0); // Quality 4 (balanced)
        av_opt_set_int(m_codecContext->priv_data, "qp", 23, 0); // Quantization parameter
    }

    // Software encoder specific settings (x264 options are invalid for HW encoders)
    if (type == Software) {
        av_opt_set(m_codecContext->priv_data, "preset", "veryfast", 0);
        av_opt_set(m_codecContext->priv_data, "tune", "zerolatency", 0);
        av_opt_set(m_codecContext->priv_data, "profile", "high", 0);
        av_opt_set(m_codecContext->priv_data, "crf", "23", 0);
        av_opt_set(m_codecContext->priv_data, "x264-params", "keyint=30:min-keyint=30", 0);
    }

    // Open codec
    int ret = avcodec_open2(m_codecContext, m_codec, nullptr);
    if (ret < 0) {
        char err[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, err, sizeof(err));
        qWarning() << "Could not open codec:" << err;
        avcodec_free_context(&m_codecContext);
        return false;
    }

    // Allocate frame
    m_frame = av_frame_alloc();
    if (!m_frame) {
        qCritical() << "Could not allocate video frame";
        avcodec_free_context(&m_codecContext);
        return false;
    }

    m_frame->format = m_codecContext->pix_fmt;
    m_frame->width = m_codecContext->width;
    m_frame->height = m_codecContext->height;

    ret = av_frame_get_buffer(m_frame, 0);
    if (ret < 0) {
        qCritical() << "Could not allocate frame buffer";
        av_frame_free(&m_frame);
        avcodec_free_context(&m_codecContext);
        return false;
    }

    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        qCritical() << "Could not allocate packet";
        return false;
    }

    return true;
}

bool HWEncoder::encodeFrame(const QPixmap& frame)
{
    if (!m_initialized) {
        return false;
    }

    QImage image = frame.toImage().convertToFormat(QImage::Format_RGB32);

    {
        std::lock_guard<std::mutex> lock(m_frameMutex);

        // CRITICAL: Keep only 1 frame - drop everything else
        if (m_frameQueue.size() >= 1) {
            // Drop old frame - we only want the latest
            while (!m_frameQueue.empty()) {
                m_frameQueue.pop();
            }
        }

        m_frameQueue.push(image);
    }

    m_frameCV.notify_one();

    return true;
}

void HWEncoder::encoderThreadFunc()
{
    while (m_running) {
        QImage image;

        {
            std::unique_lock<std::mutex> lock(m_frameMutex);
            m_frameCV.wait(lock, [this] { return !m_frameQueue.empty() || !m_running; });

            if (!m_running) break;

            if (!m_frameQueue.empty()) {
                image = m_frameQueue.front();
                m_frameQueue.pop();
            }
        }

        if (image.isNull()) continue;

        auto encodeStart = std::chrono::high_resolution_clock::now();

        // Initialize SwsContext if needed
        if (!m_swsContext) {
            m_swsContext = sws_getContext(
                image.width(), image.height(), AV_PIX_FMT_BGRA,
                m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
                SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
            );
        }

        // Convert QImage to AVFrame
        int ret = av_frame_make_writable(m_frame);
        if (ret < 0) {
            qWarning() << "Frame not writable";
            continue;
        }

        const uint8_t* srcData[1] = { image.constBits() };
        int srcLinesize[1] = { image.bytesPerLine() };

        sws_scale(m_swsContext, srcData, srcLinesize, 0, image.height(),
                  m_frame->data, m_frame->linesize);

        m_frame->pts = m_frameCount++;

        // Encode frame
        if (!encodeAVFrame(m_frame)) {
            qWarning() << "Failed to encode frame";
            continue;
        }

        auto encodeEnd = std::chrono::high_resolution_clock::now();
        double encodeTime = std::chrono::duration<double, std::milli>(encodeEnd - encodeStart).count();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.avgEncodeTimeMs = (m_stats.avgEncodeTimeMs * 0.9) + (encodeTime * 0.1);
        }
    }
}

bool HWEncoder::encodeAVFrame(AVFrame* frame)
{
    int ret = avcodec_send_frame(m_codecContext, frame);
    if (ret < 0) {
        qWarning() << "Error sending frame for encoding";
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        } else if (ret < 0) {
            qWarning() << "Error during encoding";
            return false;
        }

        // Copy packet data
        EncodedPacket encodedPkt;
        encodedPkt.data = QByteArray(reinterpret_cast<const char*>(m_packet->data), m_packet->size);
        encodedPkt.pts = m_packet->pts;
        encodedPkt.isKeyframe = (m_packet->flags & AV_PKT_FLAG_KEY) != 0;

        {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            m_packetQueue.push(encodedPkt);

            std::lock_guard<std::mutex> statsLock(m_statsMutex);
            m_stats.framesEncoded++;
            m_stats.bytesEncoded += encodedPkt.data.size();
        }

        av_packet_unref(m_packet);

        emit packetReady();
        updateStats();
    }

    return true;
}

QByteArray HWEncoder::getEncodedPacket()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    if (m_packetQueue.empty()) {
        return QByteArray();
    }

    EncodedPacket pkt = m_packetQueue.front();
    m_packetQueue.pop();

    return pkt.data;
}

HWEncoder::Stats HWEncoder::getStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void HWEncoder::updateStats()
{
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastFrameTime).count();

    if (elapsed >= 1.0) {
        std::lock_guard<std::mutex> lock(m_statsMutex);

        // Calculate FPS based on actual encoded frames in this period
        int64_t currentFrames = m_stats.framesEncoded;

        m_stats.currentFPS = (currentFrames - m_lastFrameCount) / elapsed;
        m_lastFrameCount = currentFrames;

        m_lastFrameTime = now;
    }
}

void HWEncoder::setBitrate(int bitrate)
{
    if (m_codecContext) {
        m_codecContext->bit_rate = bitrate;
        m_bitrate = bitrate;
    }
}

QByteArray HWEncoder::getExtraData() const
{
    if (m_codecContext && m_codecContext->extradata_size > 0) {
        return QByteArray(reinterpret_cast<const char*>(m_codecContext->extradata),
                         m_codecContext->extradata_size);
    }
    return QByteArray();
}
