#include "camera/CaptureWorker.h"

#include "lockin/LockInController.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#include <string>

namespace qtthermal {

CaptureWorker::CaptureWorker(Model model, bool simulate, QObject* parent)
    : QObject(parent)
    , m_backend(simulate ? makeSimulatedCameraBackend(model) : makeUsbCameraBackend(model))
{
}

CaptureWorker::~CaptureWorker() = default;

void CaptureWorker::requestStop()
{
    m_stop.store(true);
}

void CaptureWorker::setParams(const ProcessingParams& params)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_params = params;
}

void CaptureWorker::requestShutter()
{
    m_shutterRequested.store(true);
}

void CaptureWorker::requestGainMode(GainMode mode)
{
    m_pendingGain.store(static_cast<int>(mode));
    m_gainRequested.store(true);
}

void CaptureWorker::requestRawDump()
{
    m_rawDumpRequested.store(true);
}

void CaptureWorker::setLockInController(LockInController* controller)
{
    m_lockIn = controller;
}

void CaptureWorker::setLockInActive(bool active)
{
    m_lockInActive.store(active);
}

ProcessingParams CaptureWorker::snapshotParams() const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    return m_params;
}

void CaptureWorker::run()
{
    m_stop.store(false);

    std::string error;
    if (!m_backend->connect(&error)) {
        qWarning().noquote() << "Camera connection failed:" << QString::fromStdString(error);
        emit errorOccurred(QString::fromStdString(error));
        emit connectionChanged(false);
        return;
    }
    if (!m_backend->initialize(&error)) {
        qWarning().noquote() << "Camera initialization failed:" << QString::fromStdString(error);
        emit errorOccurred(QString::fromStdString(error));
        m_backend->disconnect();
        emit connectionChanged(false);
        return;
    }
    if (!m_backend->startStreaming(&error)) {
        qWarning().noquote() << "Camera streaming failed:" << QString::fromStdString(error);
        emit errorOccurred(QString::fromStdString(error));
        m_backend->disconnect();
        emit connectionChanged(false);
        return;
    }

    const DeviceInfo& info = m_backend->deviceInfo();
    qInfo().noquote() << "Camera connected:" << info.model << "firmware" << info.firmwareVersion;
    emit deviceInfoReady(info.model, info.firmwareVersion, info.partNumber, info.serial,
                         info.hardwareVersion, info.modelLong);
    emit connectionChanged(true);

    m_processor.resetTemporal();
    QElapsedTimer fpsTimer;
    fpsTimer.start();

    int consecutiveErrors = 0;
    while (!m_stop.load()) {
        if (m_gainRequested.exchange(false)) {
            const auto mode = static_cast<GainMode>(m_pendingGain.load());
            if (!m_backend->setGainMode(mode, &error)) {
                emit errorOccurred(QString::fromStdString(error));
            }
        }
        if (m_shutterRequested.exchange(false)) {
            if (!m_backend->triggerShutter(&error)) {
                emit errorOccurred(QString::fromStdString(error));
            }
        }

        Image16 thermal;
        Image8 ir;
        if (!m_backend->readFrame(thermal, ir, &error)) {
            ++consecutiveErrors;
            if (consecutiveErrors <= 3) {
                emit errorOccurred(QString::fromStdString(error));
            }
            QThread::msleep(5);
            continue;
        }
        consecutiveErrors = 0;

        ProcessingParams params = snapshotParams();
        params.gain = m_backend->gainMode();

        if (m_rawDumpRequested.exchange(false)) {
            emit rawFrameReady(QVector<quint16>(thermal.data.begin(), thermal.data.end()),
                               thermal.width, thermal.height);
        }

        LockInPanes panes;
        LockInPanes* panesPtr = nullptr;
        if (m_lockIn != nullptr) {
            if (m_lockInActive.load()) {
                m_lockIn->pushFrame(thermal);
            }
            std::optional<LockInPanes> result = m_lockIn->latest();
            if (result.has_value()) {
                panes = std::move(*result);
                panesPtr = &panes;
            }
        }

        const ProcessedFrame frame = m_processor.process(thermal, ir, params, panesPtr);

        if (!frame.image.isNull()) {
            emit frameReady(frame.image);
        }

        ++m_fpsCount;
        const qint64 elapsed = fpsTimer.elapsed();
        if (elapsed >= 1000) {
            m_fps = static_cast<double>(m_fpsCount) * 1000.0 / static_cast<double>(elapsed);
            m_fpsCount = 0;
            fpsTimer.restart();
        }

        const FrameStats stats = m_backend->stats();
        emit statusUpdated(m_fps, frame.spotTemp, stats.framesRead, stats.framesDropped);
    }

    m_backend->stopStreaming();
    m_backend->disconnect();
    emit connectionChanged(false);
}

} // namespace qtthermal
