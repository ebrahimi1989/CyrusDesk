#ifndef LATENCYMONITOR_H
#define LATENCYMONITOR_H

#include <QByteArray>
#include <QDataStream>
#include <chrono>

/**
 * Lightweight latency monitoring
 * Adds timestamp to each frame for measuring end-to-end latency
 */
class LatencyMonitor {
public:
    // Add timestamp to frame data
    static QByteArray addTimestamp(const QByteArray& data) {
        QByteArray result;
        QDataStream stream(&result, QIODevice::WriteOnly);

        // Current timestamp in microseconds
        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::time_point_cast<std::chrono::microseconds>(now)
                            .time_since_epoch()
                            .count();

        stream << static_cast<qint64>(timestamp);
        result.append(data);

        return result;
    }

    // Extract timestamp and calculate latency
    static double getLatencyMs(const QByteArray& data, QByteArray& originalData) {
        if (data.size() < static_cast<int>(sizeof(qint64))) {
            originalData = data;
            return -1.0; // Invalid
        }

        QDataStream stream(data);
        qint64 sentTimestamp;
        stream >> sentTimestamp;

        // Get remaining data
        originalData = data.mid(sizeof(qint64));

        // Calculate latency
        auto now = std::chrono::high_resolution_clock::now();
        auto currentTimestamp = std::chrono::time_point_cast<std::chrono::microseconds>(now)
                                   .time_since_epoch()
                                   .count();

        return (currentTimestamp - sentTimestamp) / 1000.0; // Convert to milliseconds
    }
};

#endif // LATENCYMONITOR_H
