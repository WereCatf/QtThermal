#pragma once

#include "core/CameraTypes.h"
#include "core/ImageProcessor.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace qtthermal {

/// Configuration for a lock-in thermography run.
struct LockInConfig {
    std::string port;
    int baudRate = 115200;
    double period = 1.0;
    double integration = 60.0;
    bool invert = false;
};

/// Default serial port for the current platform.
[[nodiscard]] std::string defaultLockInPort();

/// Runs lock-in demodulation in a background thread.
///
/// Thermal frames are pushed in by the capture worker; the controller toggles
/// an external load over the serial port and demodulates the response.
class LockInController : public QObject {
    Q_OBJECT

public:
    explicit LockInController(const ModelConfig& config, QObject* parent = nullptr);
    ~LockInController() override;

    LockInController(const LockInController&) = delete;
    LockInController& operator=(const LockInController&) = delete;

    /// Start a background integration. Returns false and sets error on failure.
    bool start(const LockInConfig& config, QString* error = nullptr);

    /// Request stop and optionally wait for the worker to finish.
    void stop(bool wait = true);

    [[nodiscard]] bool isRunning() const;

    /// Thread-safe: enqueue a thermal frame for demodulation.
    void pushFrame(const Image16& frame);

    /// Thread-safe: most recent demodulation result, if any.
    [[nodiscard]] std::optional<LockInPanes> latest() const;

signals:
    void finished();
    void progress(int percent);
    void errorOccurred(const QString& message);

private:
    void run(const LockInConfig& config);
    void publish(const ImageFloat& inPhase, const ImageFloat& quadrature, std::uint64_t totalFrames);

    ModelConfig m_config;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_running{false};

    mutable std::mutex m_dataMutex;
    LockInPanes m_result;

    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    std::deque<std::pair<double, Image16>> m_queue;
};

} // namespace qtthermal
