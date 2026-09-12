#pragma once

#include "core/CameraTypes.h"

#include <QString>

#include <memory>
#include <string>

namespace qtthermal {

/// Static device information read from the camera registers.
struct DeviceInfo {
    QString model;
    QString firmwareVersion;
    QString partNumber;
    QString serial;
    QString hardwareVersion;
    QString modelLong;
};

/// Abstract camera backend so the UI and tests can run without hardware.
class CameraBackend {
public:
    virtual ~CameraBackend() = default;

    virtual bool connect(std::string* error) = 0;
    virtual void disconnect() = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;

    virtual bool initialize(std::string* error) = 0;
    [[nodiscard]] virtual const DeviceInfo& deviceInfo() const = 0;

    virtual bool startStreaming(std::string* error) = 0;
    virtual void stopStreaming() = 0;

    virtual bool readFrame(Image16& thermal, Image8& ir, std::string* error) = 0;
    virtual bool triggerShutter(std::string* error) = 0;
    virtual bool setGainMode(GainMode mode, std::string* error) = 0;

    [[nodiscard]] virtual GainMode gainMode() const = 0;
    [[nodiscard]] virtual const ModelConfig& config() const = 0;
    [[nodiscard]] virtual const FrameStats& stats() const = 0;
};

/// Create a libusb-backed camera backend.
[[nodiscard]] std::unique_ptr<CameraBackend> makeUsbCameraBackend(Model model);

/// Create a synthetic backend for hardware-free operation.
[[nodiscard]] std::unique_ptr<CameraBackend> makeSimulatedCameraBackend(Model model);

} // namespace qtthermal
