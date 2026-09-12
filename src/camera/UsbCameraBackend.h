#pragma once

#include "camera/CameraBackend.h"
#include "core/Protocol.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace qtthermal {

/// Native USB implementation of the camera backend.
///
/// On Windows it talks to the WinUSB driver that the device is bound to; on
/// other platforms it uses libusb-1.0.
class UsbCameraBackend final : public CameraBackend {
public:
    explicit UsbCameraBackend(Model model);
    ~UsbCameraBackend() override;

    UsbCameraBackend(const UsbCameraBackend&) = delete;
    UsbCameraBackend& operator=(const UsbCameraBackend&) = delete;

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
    bool sendCommand(Command command, std::string* error);
    bool readResponse(std::size_t length, std::vector<std::uint8_t>& out, std::string* error);
    bool readStatus(std::uint8_t& status, std::string* error);
    bool readFrameBuffer(std::vector<std::uint8_t>& buffer, std::size_t readSize, std::string* error);
    bool readRegister(Command command, std::size_t length, QString& out, std::string* error);

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    ModelConfig m_config;
    DeviceInfo m_deviceInfo;
    FrameStats m_stats;
    GainMode m_gainMode = GainMode::High;
    bool m_connected = false;
    bool m_streaming = false;
    bool m_validateMarkers = true;

    std::vector<std::uint8_t> m_frameBuffer;
    std::vector<std::uint8_t> m_chunkBuffer;
    std::vector<std::uint8_t> m_frameData;
    std::vector<std::uint8_t> m_controlBuffer;
};

} // namespace qtthermal
