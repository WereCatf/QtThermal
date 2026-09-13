#pragma once

#include "camera/CameraBackend.h"
#include "core/ImageProcessor.h"

#include <QImage>
#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>
#include <memory>
#include <mutex>

namespace qtthermal {

class LockInController;

/// Reads frames on a worker thread and renders them into images.
class CaptureWorker : public QObject {
    Q_OBJECT

public:
    CaptureWorker(Model model, bool simulate, QObject* parent = nullptr);
    ~CaptureWorker() override;

    /// Thread-safe: ask the capture loop to finish.
    void requestStop();

    /// Thread-safe: update the processing parameters.
    void setParams(const ProcessingParams& params);

    /// Thread-safe: queue a shutter/NUC calibration.
    void requestShutter();

    /// Thread-safe: queue a gain mode change.
    void requestGainMode(GainMode mode);

    /// Thread-safe: queue a one-shot raw thermal frame dump.
    void requestRawDump();

    /// Thread-safe: attach or detach the lock-in controller.
    void setLockInController(LockInController* controller);
    void setLockInActive(bool active);

public slots:
    /// Runs the capture loop; intended to be invoked on the worker thread.
    void run();

signals:
    void frameReady(const QImage& image);
    void statusUpdated(double fps, double spotTemp, double minTemp, double maxTemp);
    void connectionChanged(bool connected);
    void deviceInfoReady(const QString& model, const QString& firmware, const QString& partNumber,
                         const QString& serial, const QString& hardware, const QString& modelLong);
    void rawFrameReady(const QVector<quint16>& data, int width, int height);
    void errorOccurred(const QString& message);

private:
    [[nodiscard]] ProcessingParams snapshotParams() const;

    std::unique_ptr<CameraBackend> m_backend;
    ImageProcessor m_processor;
    mutable std::mutex m_mutex;
    ProcessingParams m_params;

    std::atomic<bool> m_stop{false};
    std::atomic<bool> m_shutterRequested{false};
    std::atomic<bool> m_gainRequested{false};
    std::atomic<bool> m_rawDumpRequested{false};
    std::atomic<int> m_pendingGain{static_cast<int>(GainMode::High)};

    LockInController* m_lockIn = nullptr;
    std::atomic<bool> m_lockInActive{false};

    double m_fps = 0.0;
    int m_fpsCount = 0;
};

} // namespace qtthermal
