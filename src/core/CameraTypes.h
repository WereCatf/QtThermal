#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace qtthermal {

/// USB vendor ID shared by all supported cameras.
inline constexpr std::uint16_t kUsbVendorId = 0x3474;

/// Frame marker size in bytes.
inline constexpr int kMarkerSize = 12;

/// Raw sensor values are expressed in 1/64 Kelvin units.
inline constexpr double kTempScale = 64.0;

/// Offset between Kelvin and Celsius.
inline constexpr double kKelvinOffset = 273.15;

/// Chunk size used for incremental USB bulk reads.
inline constexpr int kFrameReadChunk = 16384;

/// Expected increment of the wrapping frame counter per frame.
inline constexpr int kCnt3Increment = 40;

/// Wrap value of the wrapping frame counter.
inline constexpr int kCnt3Wrap = 2048;

/// Supported camera models.
enum class Model {
    P1 = 0,
    P3 = 1,
};

/// Sensor gain mode.
enum class GainMode {
    Low = 0,
    High = 1,
    Auto = 2,
};

/// Model-specific constants derived from the sensor geometry.
struct ModelConfig {
    Model model = Model::P3;
    std::uint16_t pid = 0x45A2;
    int sensorWidth = 256;
    int sensorHeight = 192;
    int shutterSegment1Lines = 36;
    int shutterSegment2Lines = 800;

    [[nodiscard]] int frameRows() const { return 2 * sensorHeight + 2; }
    [[nodiscard]] int frameSize() const { return 2 * frameRows() * sensorWidth; }
    [[nodiscard]] int frameReadSize() const { return frameSize() + 2 * kMarkerSize; }
    [[nodiscard]] int frameBufferSize() const { return frameReadSize() + shutterSegment1(); }
    [[nodiscard]] int irRowEnd() const { return sensorHeight; }
    [[nodiscard]] int thermalRowStart() const { return sensorHeight + 2; }
    [[nodiscard]] int thermalRowEnd() const { return 2 * sensorHeight + 2; }
    [[nodiscard]] int shutterSegment1() const { return shutterSegment1Lines * sensorWidth; }
    [[nodiscard]] int shutterSegment2() const { return shutterSegment2Lines * sensorWidth + kMarkerSize; }
};

/// Return the configuration for a camera model.
[[nodiscard]] ModelConfig modelConfig(Model model);

/// Parse a model name ("p1" or "p3"); returns false when unrecognised.
[[nodiscard]] bool parseModel(const std::string& name, Model& out);

/// Human readable model name, e.g. "P3".
[[nodiscard]] std::string modelName(Model model);

/// Environmental parameters used for radiometric correction.
struct EnvParams {
    double emissivity = 0.95;
    double ambientTemp = 25.0;
    double reflectedTemp = 25.0;
    double distance = 1.0;
    double humidity = 0.5;
};

/// Counters describing the health of the USB frame stream.
struct FrameStats {
    std::uint64_t framesRead = 0;
    std::uint64_t framesDropped = 0;
    std::uint64_t markerMismatches = 0;
    std::uint32_t lastCnt1 = 0;
    std::uint32_t lastCnt3 = 0;
};

/// Simple row-major image container used throughout the application.
template <typename T>
struct Image {
    int width = 0;
    int height = 0;
    std::vector<T> data;

    [[nodiscard]] bool isEmpty() const { return width <= 0 || height <= 0 || data.empty(); }
    [[nodiscard]] std::size_t size() const { return data.size(); }

    void resize(int w, int h)
    {
        width = w;
        height = h;
        data.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), T{});
    }

    [[nodiscard]] T* row(int y) { return data.data() + static_cast<std::size_t>(y) * width; }
    [[nodiscard]] const T* row(int y) const
    {
        return data.data() + static_cast<std::size_t>(y) * width;
    }

    [[nodiscard]] T& at(int x, int y) { return data[static_cast<std::size_t>(y) * width + x]; }
    [[nodiscard]] const T& at(int x, int y) const
    {
        return data[static_cast<std::size_t>(y) * width + x];
    }
};

using Image8 = Image<std::uint8_t>;
using Image16 = Image<std::uint16_t>;
using ImageFloat = Image<float>;

} // namespace qtthermal
