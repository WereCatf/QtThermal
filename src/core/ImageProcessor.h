#pragma once

#include "core/CameraTypes.h"
#include "core/Colormap.h"

#include <QImage>

#include <optional>
#include <vector>

namespace qtthermal {

/// Automatic gain control strategy.
enum class AgcMode {
    Factory = 0,
    Temporal = 1,
    Fixed = 2,
};

/// 2x upscaling interpolation mode.
enum class ScaleMode {
    Off = 0,
    Nearest = 1,
    Bilinear = 2,
    Bicubic = 3,
    Lanczos = 4,
};

/// Extreme value marker mode.
enum class HotspotMode {
    Off = 0,
    Max = 1,
    Min = 2,
    MinMax = 3,
};

/// Upper limit for the number of tracked hot and cold spots.
inline constexpr int kMaxTrackedHotspots = 10;

/// All tunable parameters affecting the rendered frame.
struct ProcessingParams {
    AgcMode agcMode = AgcMode::Factory;
    ScaleMode scaleMode = ScaleMode::Bicubic;
    bool useClahe = true;
    double ddeStrength = 0.3;
    double tnrAlpha = 0.5;
    Colormap colormap = Colormap::Ironbow;
    bool mirror = false;
    int rotation = 0;
    int zoom = 3;
    bool showReticule = true;
    bool showColorbar = true;
    bool showHelp = false;
    HotspotMode hotspot = HotspotMode::Off;
    int hotspotMaxCount = 1;
    int hotspotMinCount = 1;
    double fixedRangeMin = 10.0;
    double fixedRangeMax = 40.0;
    EnvParams env;
    double fps = 0.0;
    GainMode gain = GainMode::High;
    Model model = Model::P3;
};

/// Lock-in demodulation panes, empty when unavailable.
struct LockInPanes {
    ImageFloat inPhase;
    ImageFloat quadrature;
    ImageFloat amplitude;
    ImageFloat angle;

    [[nodiscard]] bool isValid() const { return !inPhase.isEmpty() && !amplitude.isEmpty(); }
};

/// A single tracked temperature extreme.
struct Hotspot {
    int x = 0;
    int y = 0;
    double temp = 0.0;
};

/// Result of rendering a single thermal frame.
struct ProcessedFrame {
    QImage image;
    double spotTemp = 0.0;
    double minTemp = 0.0;
    double maxTemp = 0.0;
    std::vector<Hotspot> hotSpots;
    std::vector<Hotspot> coldSpots;
};

/// Convert raw thermal frames into displayable images.
class ImageProcessor {
public:
    ProcessedFrame process(const Image16& thermal, const Image8& ir, const ProcessingParams& params,
                           const LockInPanes* lockIn = nullptr);

    void resetTemporal();

private:
    Image16 m_previousFrame;
    bool m_emaInitialized = false;
    double m_emaLow = 0.0;
    double m_emaHigh = 0.0;
};

} // namespace qtthermal
