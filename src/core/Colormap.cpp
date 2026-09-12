#include "core/Colormap.h"

#include <algorithm>
#include <cmath>

namespace qtthermal {
namespace {

std::uint8_t toByte(double value)
{
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
}

std::array<QRgb, kColormapSize> buildWhiteHot()
{
    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        lut[static_cast<std::size_t>(i)] = qRgb(i, i, i);
    }
    return lut;
}

std::array<QRgb, kColormapSize> buildBlackHot()
{
    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        const int v = 255 - i;
        lut[static_cast<std::size_t>(i)] = qRgb(v, v, v);
    }
    return lut;
}

std::array<QRgb, kColormapSize> buildRainbow()
{
    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        const double x = static_cast<double>(i) / (kColormapSize - 1);
        const auto channel = [x](double offset) {
            return std::clamp(1.5 - std::abs(4.0 * x - offset), 0.0, 1.0);
        };
        const double r = channel(3.0);
        const double g = channel(2.0);
        const double b = channel(1.0);
        lut[static_cast<std::size_t>(i)] = qRgb(toByte(r * 255.0), toByte(g * 255.0), toByte(b * 255.0));
    }
    return lut;
}

struct Anchor {
    double position;
    int red;
    int green;
    int blue;
};

std::array<QRgb, kColormapSize> buildIronbow()
{
    static constexpr Anchor anchors[] = {
        {0.00, 0, 0, 0},     {0.15, 20, 0, 60},   {0.30, 60, 0, 120},
        {0.45, 130, 0, 140}, {0.60, 200, 40, 80}, {0.75, 240, 120, 20},
        {0.90, 255, 200, 60}, {1.00, 255, 255, 255},
    };
    constexpr std::size_t anchorCount = sizeof(anchors) / sizeof(anchors[0]);

    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        const double x = static_cast<double>(i) / (kColormapSize - 1);
        std::size_t upper = 1;
        while (upper < anchorCount - 1 && anchors[upper].position < x) {
            ++upper;
        }
        const Anchor& a = anchors[upper - 1];
        const Anchor& b = anchors[upper];
        const double span = b.position - a.position;
        const double t = span > 0.0 ? (x - a.position) / span : 0.0;
        const double r = a.red + (b.red - a.red) * t;
        const double g = a.green + (b.green - a.green) * t;
        const double bl = a.blue + (b.blue - a.blue) * t;
        lut[static_cast<std::size_t>(i)] = qRgb(toByte(r), toByte(g), toByte(bl));
    }
    return lut;
}

std::array<QRgb, kColormapSize> buildMilitary()
{
    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        lut[static_cast<std::size_t>(i)] =
            qRgb(toByte(i * 0.3), i, toByte(i * 0.2));
    }
    return lut;
}

std::array<QRgb, kColormapSize> buildSepia()
{
    std::array<QRgb, kColormapSize> lut{};
    for (int i = 0; i < kColormapSize; ++i) {
        lut[static_cast<std::size_t>(i)] =
            qRgb(i, toByte(i * 0.7), toByte(i * 0.4));
    }
    return lut;
}

} // namespace

const std::array<QRgb, kColormapSize>& colormapLut(Colormap colormap)
{
    static const std::array<QRgb, kColormapSize> whiteHot = buildWhiteHot();
    static const std::array<QRgb, kColormapSize> blackHot = buildBlackHot();
    static const std::array<QRgb, kColormapSize> rainbow = buildRainbow();
    static const std::array<QRgb, kColormapSize> ironbow = buildIronbow();
    static const std::array<QRgb, kColormapSize> military = buildMilitary();
    static const std::array<QRgb, kColormapSize> sepia = buildSepia();

    switch (colormap) {
    case Colormap::WhiteHot:
        return whiteHot;
    case Colormap::BlackHot:
        return blackHot;
    case Colormap::Rainbow:
        return rainbow;
    case Colormap::Ironbow:
        return ironbow;
    case Colormap::Military:
        return military;
    case Colormap::Sepia:
        return sepia;
    }
    return ironbow;
}

QImage applyColormap(const Image8& gray, Colormap colormap)
{
    if (gray.isEmpty()) {
        return {};
    }

    const auto& lut = colormapLut(colormap);
    QImage image(gray.width, gray.height, QImage::Format_RGB32);
    for (int y = 0; y < gray.height; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        const std::uint8_t* source = gray.row(y);
        for (int x = 0; x < gray.width; ++x) {
            scanLine[x] = lut[source[x]];
        }
    }
    return image;
}

QString colormapName(Colormap colormap)
{
    switch (colormap) {
    case Colormap::WhiteHot:
        return QStringLiteral("White Hot");
    case Colormap::BlackHot:
        return QStringLiteral("Black Hot");
    case Colormap::Rainbow:
        return QStringLiteral("Rainbow");
    case Colormap::Ironbow:
        return QStringLiteral("Ironbow");
    case Colormap::Military:
        return QStringLiteral("Military");
    case Colormap::Sepia:
        return QStringLiteral("Sepia");
    }
    return QStringLiteral("Ironbow");
}

} // namespace qtthermal
