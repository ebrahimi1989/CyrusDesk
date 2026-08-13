#include "hwdecoder.h"
#include <QDebug>
#include <chrono>
#include <cstring>

HWDecoder::HWDecoder(QObject* parent)
    : QObject(parent)
    , m_codecContext(nullptr)
    , m_codec(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
    , m_swsContext(nullptr)
    , m_hwDeviceContext(nullptr)
    , m_hwType(Auto)
    , m_hwAccelEnabled(false)
    , m_running(false)
    , m_lastFrameCount(0)
    , m_initialized(false)
{
    memset(&m_stats, 0, sizeof(Stats));
}

HWDecoder::~HWDecoder()
{
    qDebug() << "HWDecoder destructor called";

    // Stop decoder thread
    m_running = false;

    // Wake up threads
    m_packetCV.notify_all();
    m_queueCV.notify_all();

    // Clear queues
    {
        std::lock_guard<std::mutex> lock1(m_packetMutex);
        std::lock_guard<std::mutex> lock2(m_queueMutex);
        while (!m_packetQueue.empty()) m_packetQueue.pop();
        while (!m_frameQueue.empty()) m_frameQueue.pop();
    }

    // Wait for thread to finish
    if (m_decoderThread && m_decoderThread->joinable()) {
        qDebug() << "Waiting for decoder thread to finish...";
        m_decoderThread->join();
        qDebug() << "Decoder thread finished";
    }

    // Flush decoder
    if (m_codecContext && m_initialized) {
        avcodec_send_packet(m_codecContext, nullptr); // Flush
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

    qDebug() << "HWDecoder cleanup complete";
}

bool HWDecoder::initialize(HWAccelType type)
{
    m_hwType = type;

    // Try hardware acceleration first
    if (type == Auto) {
        // Try CUDA first (NVIDIA - fastest)
        if (initializeCodec(CUDA)) {
            qDebug() << "Initialized with CUDA decoder";
            m_hwType = CUDA;
        }
        // Try VAAPI (good for Intel/AMD on Linux)
        else if (initializeCodec(VAAPI)) {
            qDebug() << "Initialized with VAAPI decoder";
            m_hwType = VAAPI;
        }
        // Try QSV (Intel Quick Sync)
        else if (initializeCodec(QSV)) {
            qDebug() << "Initialized with QSV decoder";
            m_hwType = QSV;
        }
        // Try D3D11VA (Windows)
        else if (initializeCodec(D3D11VA)) {
            qDebug() << "Initialized with D3D11VA decoder";
            m_hwType = D3D11VA;
        }
        // Fallback to software
        else if (initializeCodec(Software)) {
            qDebug() << "Initialized with Software decoder";
            m_hwType = Software;
        }
        else {
            qCritical() << "Failed to initialize any decoder";
            return false;
        }
    } else {
        if (!initializeCodec(type)) {
            qCritical() << "Failed to initialize requested decoder";
            return false;
        }
    }

    // Start decoder thread
    m_running = true;
    m_decoderThread = std::make_unique<std::thread>(&HWDecoder::decoderThreadFunc, this);

    m_initialized = true;
    m_lastFrameTime = std::chrono::steady_clock::now();

    return true;
}

AVHWDeviceType HWDecoder::getHWDeviceType(HWAccelType type)
{
    switch (type) {
        case CUDA: return AV_HWDEVICE_TYPE_CUDA;
        case VAAPI: return AV_HWDEVICE_TYPE_VAAPI;
        case QSV: return AV_HWDEVICE_TYPE_QSV;
        case DXVA2: return AV_HWDEVICE_TYPE_DXVA2;
        case D3D11VA: return AV_HWDEVICE_TYPE_D3D11VA;
        default: return AV_HWDEVICE_TYPE_NONE;
    }
}

bool HWDecoder::initializeHardwareAccel(AVHWDeviceType hwType)
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

bool HWDecoder::initializeCodec(HWAccelType type)
{
    // Find H.264 decoder
    m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!m_codec) {
        qCritical() << "H.264 decoder not found";
        return false;
    }

    m_codecContext = avcodec_alloc_context3(m_codec);
    if (!m_codecContext) {
        qCritical() << "Could not allocate video codec context";
        return false;
    }

    // Low latency configuration
    m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_codecContext->flags2 |= AV_CODEC_FLAG2_FAST;
    m_codecContext->thread_count = 1; // Single thread for lowest latency
    m_codecContext->delay = 0; // Zero frame delay

    // Hardware acceleration setup
    if (type != Software) {
        AVHWDeviceType hwType = getHWDeviceType(type);

        // Check if hardware decoder is available
        for (int i = 0;; i++) {
            const AVCodecHWConfig* config = avcodec_get_hw_config(m_codec, i);
            if (!config) {
                qWarning() << "Decoder does not support hardware type";
                avcodec_free_context(&m_codecContext);
                return false;
            }

            if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
                config->device_type == hwType) {
                m_codecContext->pix_fmt = config->pix_fmt;
                break;
            }
        }

        if (!initializeHardwareAccel(hwType)) {
            avcodec_free_context(&m_codecContext);
            return false;
        }
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

    // Allocate packet
    m_packet = av_packet_alloc();
    if (!m_packet) {
        qCritical() << "Could not allocate packet";
        return false;
    }

    return true;
}

bool HWDecoder::decodePacket(const QByteArray& packet)
{
    if (!m_initialized || packet.isEmpty()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_packetMutex);

        // CRITICAL: Drop old packets to prevent queue buildup and latency
        // Keep maximum 2 packets in queue
        while (m_packetQueue.size() >= 2) {
            m_packetQueue.pop();
        }

        m_packetQueue.push(packet);
    }

    m_packetCV.notify_one();

    return true;
}

void HWDecoder::decoderThreadFunc()
{
    while (m_running) {
        QByteArray packetData;

        {
            std::unique_lock<std::mutex> lock(m_packetMutex);
            m_packetCV.wait(lock, [this] { return !m_packetQueue.empty() || !m_running; });

            if (!m_running) break;

            if (!m_packetQueue.empty()) {
                packetData = m_packetQueue.front();
                m_packetQueue.pop();
            }
        }

        if (packetData.isEmpty()) continue;

        auto decodeStart = std::chrono::high_resolution_clock::now();

        // Prepare packet: copy into the packet buffer so it stays valid
        // beyond this scope (packetData dies here every iteration)
        av_packet_unref(m_packet);
        if (av_new_packet(m_packet, packetData.size()) < 0) {
            qWarning() << "Failed to allocate packet buffer";
            continue;
        }
        std::memcpy(m_packet->data, packetData.constData(), static_cast<size_t>(packetData.size()));

        // Decode packet
        if (!decodeAVPacket(m_packet)) {
            qWarning() << "Failed to decode packet";
            continue;
        }

        auto decodeEnd = std::chrono::high_resolution_clock::now();
        double decodeTime = std::chrono::duration<double, std::milli>(decodeEnd - decodeStart).count();

        // Update statistics
        {
            std::lock_guard<std::mutex> lock(m_statsMutex);
            m_stats.avgDecodeTimeMs = (m_stats.avgDecodeTimeMs * 0.9) + (decodeTime * 0.1);
            m_stats.bytesDecoded += packetData.size();
        }
    }
}

bool HWDecoder::decodeAVPacket(AVPacket* packet)
{
    int ret = avcodec_send_packet(m_codecContext, packet);
    if (ret < 0) {
        qWarning() << "Error sending packet for decoding";
        return false;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(m_codecContext, m_frame);

        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return true;
        } else if (ret < 0) {
            qWarning() << "Error during decoding";
            return false;
        }

        // Convert frame to QImage
        QImage image = avFrameToQImage(m_frame);

        if (!image.isNull()) {
            DecodedFrame decodedFrame;
            decodedFrame.image = image;
            decodedFrame.pts = m_frame->pts;

            {
                std::lock_guard<std::mutex> lock(m_queueMutex);

                // CRITICAL: Drop all old frames, keep ONLY the latest
                while (!m_frameQueue.empty()) {
                    m_frameQueue.pop();
                }

                m_frameQueue.push(decodedFrame);

                std::lock_guard<std::mutex> statsLock(m_statsMutex);
                m_stats.framesDecoded++;
            }

            emit frameReady();
            updateStats();
        }
    }

    return true;
}

QImage HWDecoder::avFrameToQImage(AVFrame* frame)
{
    AVFrame* swFrame = frame;
    AVFrame* tmpFrame = nullptr;

    // Transfer from GPU to CPU if hardware decoding
    if (m_hwAccelEnabled && frame->format != AV_PIX_FMT_YUV420P) {
        tmpFrame = av_frame_alloc();
        if (!tmpFrame) {
            return QImage();
        }

        int ret = av_hwframe_transfer_data(tmpFrame, frame, 0);
        if (ret < 0) {
            av_frame_free(&tmpFrame);
            return QImage();
        }
        swFrame = tmpFrame;
    }

    // Initialize SwsContext if needed
    if (!m_swsContext) {
        m_swsContext = sws_getContext(
            swFrame->width, swFrame->height, static_cast<AVPixelFormat>(swFrame->format),
            swFrame->width, swFrame->height, AV_PIX_FMT_RGB32,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr
        );
    }

    // Create QImage
    QImage image(swFrame->width, swFrame->height, QImage::Format_RGB32);

    uint8_t* dstData[1] = { image.bits() };
    int dstLinesize[1] = { image.bytesPerLine() };

    sws_scale(m_swsContext, swFrame->data, swFrame->linesize, 0, swFrame->height,
              dstData, dstLinesize);

    if (tmpFrame) {
        av_frame_free(&tmpFrame);
    }

    return image;
}

QPixmap HWDecoder::getDecodedFrame()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    if (m_frameQueue.empty()) {
        return QPixmap();
    }

    DecodedFrame frame = m_frameQueue.front();
    m_frameQueue.pop();

    return QPixmap::fromImage(frame.image);
}

HWDecoder::Stats HWDecoder::getStats() const
{
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

void HWDecoder::updateStats()
{
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - m_lastFrameTime).count();

    if (elapsed >= 1.0) {
        std::lock_guard<std::mutex> lock(m_statsMutex);

        // Calculate FPS based on actual decoded frames in this period
        int64_t currentFrames = m_stats.framesDecoded;

        m_stats.currentFPS = (currentFrames - m_lastFrameCount) / elapsed;
        m_lastFrameCount = currentFrames;

        m_lastFrameTime = now;
    }
}
