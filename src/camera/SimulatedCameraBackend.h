#pragma once

#include "camera/CameraBackend.h"

#include <cstdint>
#include <random>

namespace qtthermal {

/// Synthetic camera backend that produces plausible thermal frames.
///
/// It allows the application and its tests to run without USB hardware.
class SimulatedCameraBackend final : public CameraBackend {
public:
    explicit SimulatedCameraBackend(Model model);

    bool connect(std::string* error) override;
    void disconnect() override;
    [[nodiscard]] bool isConnected() const override;

    bool initialize(std::string* error) override;
    [[nodiscard]] const DeviceInfo& deviceInfo() const override;

    bool startStreaming(std::string* error) override;
    void stopStreaming() override;

    bool readFrame(Image16& thermal, Image8& ir, std::string* error) override;
    bool triggerShutter(std::string* error) override;
    bool setGainMode(GainMode mode, std::string* error) override;

    [[nodiscard]] GainMode gainMode() const override;
    [[nodiscard]] const ModelConfig& config() const override;
    [[nodiscard]] const FrameStats& stats() const override;

private:
    ModelConfig m_config;
    DeviceInfo m_deviceInfo;
    FrameStats m_stats;
    GainMode m_gainMode = GainMode::High;
    bool m_connected = false;
    bool m_streaming = false;
    std::uint32_t m_frameIndex = 0;
    std::mt19937 m_random{0x5eed1234};
};

} // namespace qtthermal
