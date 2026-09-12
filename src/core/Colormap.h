#pragma once

#include "core/CameraTypes.h"

#include <QImage>
#include <QString>

#include <array>

namespace qtthermal {

/// Available false-colour palettes.
enum class Colormap {
    WhiteHot = 0,
    BlackHot = 1,
    Rainbow = 2,
    Ironbow = 3,
    Military = 4,
    Sepia = 5,
};

/// Number of entries in every palette.
inline constexpr int kColormapSize = 256;

/// Return the 256-entry RGB lookup table for a palette.
[[nodiscard]] const std::array<QRgb, kColormapSize>& colormapLut(Colormap colormap);

/// Apply a palette to an 8-bit greyscale image.
[[nodiscard]] QImage applyColormap(const Image8& gray, Colormap colormap);

/// Human readable palette name.
[[nodiscard]] QString colormapName(Colormap colormap);

} // namespace qtthermal
