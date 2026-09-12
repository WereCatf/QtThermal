#pragma once

#include "core/CameraTypes.h"

#include <cstdint>
#include <span>

namespace qtthermal {

/// Decoded 12-byte frame marker.
struct Marker {
    std::uint8_t length = 0;
    std::uint8_t sync = 0;
    std::uint32_t cnt1 = 0;
    std::uint32_t cnt2 = 0;
    std::uint16_t cnt3 = 0;
};

/// Parse a 12-byte frame marker.
[[nodiscard]] Marker parseMarker(const std::uint8_t* data);

/// Extract the thermal image from frame data (start marker + pixel data).
[[nodiscard]] bool extractThermalData(std::span<const std::uint8_t> frame, const ModelConfig& config,
                                      Image16& out);

/// Extract the hardware AGC'd IR brightness image from frame data.
[[nodiscard]] bool extractIrBrightness(std::span<const std::uint8_t> frame, const ModelConfig& config,
                                       Image8& out);

/// Extract both the IR brightness and thermal images from frame data.
[[nodiscard]] bool extractBoth(std::span<const std::uint8_t> frame, const ModelConfig& config,
                               Image8& ir, Image16& thermal);

} // namespace qtthermal
