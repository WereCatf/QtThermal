#include "lockin/LockInController.h"

#include "core/SerialPort.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

namespace qtthermal {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTickRate = 25.0;

} // namespace

std::string defaultLockInPort()
{
#ifdef _WIN32
    return "COM3";
#else
    return "/dev/ttyACM0";
#endif
}

LockInController::LockInController(const ModelConfig& config, QObject* parent)
    : QObject(parent)
    , m_config(config)
{
}

LockInController::~LockInController()
{
    stop(true);
}

bool LockInController::start(const LockInConfig& config, QString* error)
{
    if (m_running.load()) {
        return true;
    }

    SerialPort probe;
    std::string probeError;
    if (!probe.open(config.port, config.baudRate, &probeError)) {
        if (error != nullptr) {
            *error = QString::fromStdString(probeError);
        }
        return false;
    }
    probe.close();

    {
        const std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.clear();
    }
    {
        const std::lock_guard<std::mutex> lock(m_dataMutex);
        m_result = LockInPanes{};
    }

    m_stop.store(false);
    m_running.store(true);
    m_thread = std::thread([this, config] { run(config); });
    return true;
}

void LockInController::stop(bool wait)
{
    m_stop.store(true);
    m_queueCondition.notify_all();
    if (wait && m_thread.joinable()) {
        m_thread.join();
    }
    m_running.store(false);
}

bool LockInController::isRunning() const
{
    return m_running.load();
}

void LockInController::pushFrame(const Image16& frame)
{
    if (!m_running.load()) {
        return;
    }

    const double timestamp =
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    {
        const std::lock_guard<std::mutex> lock(m_queueMutex);
        m_queue.emplace_back(timestamp, frame);
        if (m_queue.size() > 500) {
            m_queue.pop_front();
        }
    }
    m_queueCondition.notify_one();
}

std::optional<LockInPanes> LockInController::latest() const
{
    const std::lock_guard<std::mutex> lock(m_dataMutex);
    if (!m_result.isValid()) {
        return std::nullopt;
    }
    return m_result;
}

void LockInController::publish(const ImageFloat& inPhase, const ImageFloat& quadrature,
                               std::uint64_t totalFrames)
{
    if (totalFrames == 0) {
        return;
    }

    LockInPanes result;
    result.inPhase.resize(inPhase.width, inPhase.height);
    result.quadrature.resize(quadrature.width, quadrature.height);
    result.amplitude.resize(inPhase.width, inPhase.height);
    result.angle.resize(inPhase.width, inPhase.height);

    const double inverse = 1.0 / static_cast<double>(totalFrames);
    for (std::size_t i = 0; i < inPhase.data.size(); ++i) {
        const double real = inPhase.data[i] * inverse;
        const double imaginary = quadrature.data[i] * inverse;
        result.inPhase.data[i] = static_cast<float>(real);
        result.quadrature.data[i] = static_cast<float>(imaginary);
        result.amplitude.data[i] = static_cast<float>(std::sqrt(real * real + imaginary * imaginary));
        result.angle.data[i] = static_cast<float>(std::atan2(imaginary, real));
    }

    const std::lock_guard<std::mutex> lock(m_dataMutex);
    m_result = std::move(result);
}

void LockInController::run(const LockInConfig& config)
{
    using Clock = std::chrono::steady_clock;

    const int width = m_config.sensorWidth;
    const int height = m_config.sensorHeight;
    ImageFloat inPhase;
    ImageFloat quadrature;
    inPhase.resize(width, height);
    quadrature.resize(width, height);

    SerialPort port;
    std::string portError;
    if (!port.open(config.port, config.baudRate, &portError)) {
        emit errorOccurred(QString::fromStdString(portError));
        emit finished();
        m_running.store(false);
        return;
    }

    const auto writeState = [&port, &config](bool on) {
        const std::string value = (on != config.invert) ? "1\n" : "0\n";
        port.write(value);
    };

    const auto start = Clock::now();
    const double startSeconds =
        std::chrono::duration<double>(start.time_since_epoch()).count();
    auto lastToggle = start;

    double period = std::max(1e-6, config.period);
    const double tickInterval = 1.0 / kTickRate;
    double halfPeriod = period / 2.0;
    const auto frameCount = static_cast<long>(std::ceil(halfPeriod / tickInterval));
    const double adjustedHalfPeriod = std::max(halfPeriod, frameCount * tickInterval);
    if (adjustedHalfPeriod > halfPeriod) {
        period = adjustedHalfPeriod * 2.0;
        halfPeriod = period / 2.0;
    }

    double integration = std::max(1e-6, config.integration);
    const auto periodCount = static_cast<long>(std::ceil(integration / period));
    integration = std::max(integration, periodCount * period);
    const double omega = 2.0 * kPi / period;

    bool loadOn = true;
    writeState(loadOn);
    std::uint64_t totalFrames = 0;
    int lastProgress = -1;

    while (!m_stop.load()) {
        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        if (elapsed >= integration) {
            break;
        }

        const auto now = Clock::now();
        if (std::chrono::duration<double>(now - lastToggle).count() >= halfPeriod) {
            loadOn = !loadOn;
            writeState(loadOn);
            lastToggle += std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>(halfPeriod));
            if (loadOn && totalFrames > 0) {
                publish(inPhase, quadrature, totalFrames);
            }
        }

        Image16 frame;
        double timestamp = 0.0;
        bool haveFrame = false;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            if (m_queue.empty()) {
                m_queueCondition.wait_for(lock, std::chrono::milliseconds(50));
            }
            if (!m_queue.empty()) {
                timestamp = m_queue.front().first;
                frame = std::move(m_queue.front().second);
                m_queue.pop_front();
                haveFrame = true;
            }
        }
        if (!haveFrame) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const double phase = omega * (timestamp - startSeconds);
        const double sine = std::sin(phase);
        const double cosine = std::cos(phase);
        for (std::size_t i = 0; i < frame.data.size(); ++i) {
            const double value = frame.data[i];
            inPhase.data[i] += static_cast<float>(2.0 * value * cosine);
            quadrature.data[i] += static_cast<float>(2.0 * value * sine);
        }
        ++totalFrames;

        const int percent = static_cast<int>(elapsed / integration * 100.0);
        if (percent != lastProgress) {
            lastProgress = percent;
            emit progress(std::clamp(percent, 0, 100));
        }
    }

    writeState(false);
    publish(inPhase, quadrature, totalFrames);
    port.close();
    emit finished();
    m_running.store(false);
}

} // namespace qtthermal
