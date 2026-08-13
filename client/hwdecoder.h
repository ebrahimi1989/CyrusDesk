#ifndef HWDECODER_H
#define HWDECODER_H

#include <QObject>
#include <QPixmap>
#include <QImage>
#include <memory>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
}

/**
 * Hardware-accelerated H.264 decoder using FFmpeg
 * Supports hardware decoding on NVIDIA, Intel, AMD GPUs
 * Ultra-low latency configuration with zero-copy where possible
 */
class HWDecoder : public QObject {
    Q_OBJECT

public:
    enum HWAccelType {
        Auto,       // Auto-detect best available
        CUDA,       // NVIDIA GPU
        VAAPI,      // Intel/AMD (Linux)
        QSV,        // Intel Quick Sync
        DXVA2,      // DirectX Video Acceleration (Windows)
        D3D11VA,    // Direct3D 11 (Windows)
        Software    // CPU fallback
    };

    explicit HWDecoder(QObject* parent = nullptr);
    ~HWDecoder();

    // Initialize decoder
    bool initialize(HWAccelType type = Auto);

    // Decode packet (non-blocking)
    bool decodePacket(const QByteArray& packet);

    // Get decoded frame if available
    QPixmap getDecodedFrame();

    // Check if decoder is ready
    bool isReady() const { return m_initialized; }

    // Get decoding statistics
    struct Stats {
        int64_t framesDecoded;
        int64_t bytesDecoded;
        double avgDecodeTimeMs;
        double currentFPS;
    };
    Stats getStats() const;

signals:
    void frameReady();
    void error(const QString& message);

private:
    // FFmpeg context
    AVCodecContext* m_codecContext;
    const AVCodec* m_codec;
    AVFrame* m_frame;
    AVPacket* m_packet;
    SwsContext* m_swsContext;
    AVBufferRef* m_hwDeviceContext;

    // Hardware acceleration
    HWAccelType m_hwType;
    bool m_hwAccelEnabled;

    // Thread-safe frame queue
    struct DecodedFrame {
        QImage image;
        int64_t pts;
    };
    std::queue<DecodedFrame> m_frameQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;

    // Async decoding
    std::unique_ptr<std::thread> m_decoderThread;
    std::queue<QByteArray> m_packetQueue;
    std::mutex m_packetMutex;
    std::condition_variable m_packetCV;
    std::atomic<bool> m_running;

    // Statistics
    mutable std::mutex m_statsMutex;
    Stats m_stats;
    std::chrono::steady_clock::time_point m_lastFrameTime;
    int64_t m_lastFrameCount; // Instance-specific (previously static)

    bool m_initialized;

    // Private methods
    bool initializeCodec(HWAccelType type);
    bool initializeHardwareAccel(AVHWDeviceType hwType);
    void decoderThreadFunc();
    bool decodeAVPacket(AVPacket* packet);
    AVHWDeviceType getHWDeviceType(HWAccelType type);
    QImage avFrameToQImage(AVFrame* frame);
    void updateStats();
};

#endif // HWDECODER_H
