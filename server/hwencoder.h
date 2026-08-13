#ifndef HWENCODER_H
#define HWENCODER_H

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
 * Hardware-accelerated H.264 encoder using FFmpeg
 * Supports NVENC (NVIDIA), VAAPI (Intel/AMD), QSV (Intel), AMF (AMD)
 * Ultra-low latency configuration with zero-copy where possible
 */
class HWEncoder : public QObject {
    Q_OBJECT

public:
    enum HWAccelType {
        Auto,       // Auto-detect best available
        NVENC,      // NVIDIA GPU
        VAAPI,      // Intel/AMD (Linux)
        QSV,        // Intel Quick Sync
        AMF,        // AMD Media Framework
        Software    // CPU fallback
    };

    explicit HWEncoder(QObject* parent = nullptr);
    ~HWEncoder();

    // Initialize encoder with resolution and hardware acceleration
    bool initialize(int width, int height, int fps = 60, int bitrate = 8000000, HWAccelType type = Auto);

    // Encode a frame (non-blocking, returns immediately)
    bool encodeFrame(const QPixmap& frame);

    // Get encoded packet if available
    QByteArray getEncodedPacket();

    // Check if encoder is ready
    bool isReady() const { return m_initialized; }

    // Get encoding statistics
    struct Stats {
        int64_t framesEncoded;
        int64_t bytesEncoded;
        double avgEncodeTimeMs;
        double currentFPS;
    };
    Stats getStats() const;

    // Reconfigure bitrate dynamically (for adaptive streaming)
    void setBitrate(int bitrate);

    // Get codec configuration data (SPS/PPS for H.264)
    QByteArray getExtraData() const;

signals:
    void packetReady();
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

    // Configuration
    int m_width;
    int m_height;
    int m_fps;
    int m_bitrate;
    int64_t m_frameCount;

    // Thread-safe packet queue
    struct EncodedPacket {
        QByteArray data;
        int64_t pts;
        bool isKeyframe;
    };
    std::queue<EncodedPacket> m_packetQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCV;

    // Async encoding
    std::unique_ptr<std::thread> m_encoderThread;
    std::queue<QImage> m_frameQueue;
    std::mutex m_frameMutex;
    std::condition_variable m_frameCV;
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
    void encoderThreadFunc();
    bool encodeAVFrame(AVFrame* frame);
    AVHWDeviceType getHWDeviceType(HWAccelType type);
    const char* getEncoderName(HWAccelType type);
    void updateStats();
};

#endif // HWENCODER_H
