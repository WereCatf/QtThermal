#include "camera/SimulatedCameraBackend.h"

#include "core/Temperature.h"

#include <algorithm>
#include <cmath>

namespace qtthermal {
namespace {

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

SimulatedCameraBackend::SimulatedCameraBackend(Model model)
    : m_config(modelConfig(model))
{
    m_deviceInfo.model = QString::fromStdString(modelName(model));
    m_deviceInfo.firmwareVersion = QStringLiteral("00.00.02.17");
    m_deviceInfo.partNumber = QStringLiteral("SIM-0000");
    m_deviceInfo.serial = QStringLiteral("SIMULATED");
    m_deviceInfo.hardwareVersion = QStringLiteral("SIM-00.00");
    m_deviceInfo.modelLong = QStringLiteral("Simulated thermal camera");
}

bool SimulatedCameraBackend::connect(std::string* error)
{
    Q_UNUSED(error);
    m_connected = true;
    return true;
}

void SimulatedCameraBackend::disconnect()
{
    m_streaming = false;
    m_connected = false;
}

bool SimulatedCameraBackend::isConnected() const
{
    return m_connected;
}

bool SimulatedCameraBackend::initialize(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }
    m_stats = FrameStats();
    return true;
}

const DeviceInfo& SimulatedCameraBackend::deviceInfo() const
{
    return m_deviceInfo;
}

bool SimulatedCameraBackend::startStreaming(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }
    m_streaming = true;
    return true;
}

void SimulatedCameraBackend::stopStreaming()
{
    m_streaming = false;
}

bool SimulatedCameraBackend::readFrame(Image16& thermal, Image8& ir, std::string* error)
{
    if (!m_connected || !m_streaming) {
        setError(error, "Camera is not streaming");
        return false;
    }

    thermal.resize(m_config.sensorWidth, m_config.sensorHeight);
    ir.resize(m_config.sensorWidth, m_config.sensorHeight);

    const double baseTemp = 25.0 + 3.0 * std::sin(m_frameIndex * 0.02);
    const double hotX = 0.5 + 0.35 * std::cos(m_frameIndex * 0.03);
    const double hotY = 0.5 + 0.35 * std::sin(m_frameIndex * 0.027);

    std::uniform_real_distribution<double> noise(-0.25, 0.25);
    std::uint16_t minRaw = 0xFFFF;
    std::uint16_t maxRaw = 0;

    for (int y = 0; y < m_config.sensorHeight; ++y) {
        for (int x = 0; x < m_config.sensorWidth; ++x) {
            const double nx = static_cast<double>(x) / m_config.sensorWidth;
            const double ny = static_cast<double>(y) / m_config.sensorHeight;
            const double gradient = 4.0 * nx + 2.0 * ny;
            const double dx = nx - hotX;
            const double dy = ny - hotY;
            const double hotspot = 12.0 * std::exp(-(dx * dx + dy * dy) * 40.0);
            const double temperature = baseTemp + gradient + hotspot + noise(m_random);
            const auto raw = static_cast<std::uint16_t>(std::clamp(celsiusToRaw(temperature), 0, 0xFFFF));
            thermal.at(x, y) = raw;
            minRaw = std::min(minRaw, raw);
            maxRaw = std::max(maxRaw, raw);
        }
    }

    const double span = std::max(1, static_cast<int>(maxRaw) - static_cast<int>(minRaw));
    for (std::size_t i = 0; i < thermal.data.size(); ++i) {
        const double normalized = (thermal.data[i] - minRaw) / span;
        ir.data[i] = static_cast<std::uint8_t>(std::lround(std::clamp(normalized, 0.0, 1.0) * 255.0));
    }

    ++m_frameIndex;
    m_stats.framesRead += 1;
    return true;
}

bool SimulatedCameraBackend::triggerShutter(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }
    return true;
}

bool SimulatedCameraBackend::setGainMode(GainMode mode, std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }
    m_gainMode = mode;
    return true;
}

GainMode SimulatedCameraBackend::gainMode() const
{
    return m_gainMode;
}

const ModelConfig& SimulatedCameraBackend::config() const
{
    return m_config;
}

const FrameStats& SimulatedCameraBackend::stats() const
{
    return m_stats;
}

std::unique_ptr<CameraBackend> makeSimulatedCameraBackend(Model model)
{
    return std::make_unique<SimulatedCameraBackend>(model);
}

} // namespace qtthermal
