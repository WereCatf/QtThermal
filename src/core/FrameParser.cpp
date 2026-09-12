#include "core/FrameParser.h"

#include <cstring>

namespace qtthermal {
namespace {

std::uint16_t readU16(const std::uint8_t* data)
{
    return static_cast<std::uint16_t>(data[0]) | (static_cast<std::uint16_t>(data[1]) << 8);
}

std::uint32_t readU32(const std::uint8_t* data)
{
    return static_cast<std::uint32_t>(data[0]) | (static_cast<std::uint32_t>(data[1]) << 8) |
           (static_cast<std::uint32_t>(data[2]) << 16) | (static_cast<std::uint32_t>(data[3]) << 24);
}

const std::uint8_t* pixelData(std::span<const std::uint8_t> frame, const ModelConfig& config)
{
    if (frame.size() < static_cast<std::size_t>(kMarkerSize + config.frameSize())) {
        return nullptr;
    }
    return frame.data() + kMarkerSize;
}

} // namespace

Marker parseMarker(const std::uint8_t* data)
{
    Marker marker;
    marker.length = data[0];
    marker.sync = data[1];
    marker.cnt1 = readU32(data + 2);
    marker.cnt2 = readU32(data + 6);
    marker.cnt3 = readU16(data + 10);
    return marker;
}

bool extractThermalData(std::span<const std::uint8_t> frame, const ModelConfig& config, Image16& out)
{
    const std::uint8_t* pixels = pixelData(frame, config);
    if (pixels == nullptr) {
        return false;
    }

    out.resize(config.sensorWidth, config.sensorHeight);
    for (int row = 0; row < config.sensorHeight; ++row) {
        const int sourceRow = config.thermalRowStart() + row;
        const std::uint8_t* source = pixels + static_cast<std::size_t>(sourceRow) * config.sensorWidth * 2;
        std::uint16_t* target = out.row(row);
        for (int col = 0; col < config.sensorWidth; ++col) {
            target[col] = readU16(source + static_cast<std::size_t>(col) * 2);
        }
    }
    return true;
}

bool extractIrBrightness(std::span<const std::uint8_t> frame, const ModelConfig& config, Image8& out)
{
    const std::uint8_t* pixels = pixelData(frame, config);
    if (pixels == nullptr) {
        return false;
    }

    out.resize(config.sensorWidth, config.sensorHeight);
    for (int row = 0; row < config.sensorHeight; ++row) {
        const std::uint8_t* source = pixels + static_cast<std::size_t>(row) * config.sensorWidth * 2;
        std::uint8_t* target = out.row(row);
        for (int col = 0; col < config.sensorWidth; ++col) {
            target[col] = source[static_cast<std::size_t>(col) * 2];
        }
    }
    return true;
}

bool extractBoth(std::span<const std::uint8_t> frame, const ModelConfig& config, Image8& ir,
                 Image16& thermal)
{
    const std::uint8_t* pixels = pixelData(frame, config);
    if (pixels == nullptr) {
        return false;
    }

    ir.resize(config.sensorWidth, config.sensorHeight);
    thermal.resize(config.sensorWidth, config.sensorHeight);
    for (int row = 0; row < config.sensorHeight; ++row) {
        const std::uint8_t* irSource = pixels + static_cast<std::size_t>(row) * config.sensorWidth * 2;
        std::uint8_t* irTarget = ir.row(row);
        for (int col = 0; col < config.sensorWidth; ++col) {
            irTarget[col] = irSource[static_cast<std::size_t>(col) * 2];
        }

        const int thermalRow = config.thermalRowStart() + row;
        const std::uint8_t* thermalSource =
            pixels + static_cast<std::size_t>(thermalRow) * config.sensorWidth * 2;
        std::uint16_t* thermalTarget = thermal.row(row);
        for (int col = 0; col < config.sensorWidth; ++col) {
            thermalTarget[col] = readU16(thermalSource + static_cast<std::size_t>(col) * 2);
        }
    }
    return true;
}

} // namespace qtthermal
