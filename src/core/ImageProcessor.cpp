#include "core/ImageProcessor.h"

#include "core/Temperature.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QStringList>
#include <QTransform>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

namespace qtthermal {
namespace {

constexpr double kPi = 3.14159265358979323846;

constexpr QRgb kColorText = qRgb(255, 255, 255);
constexpr QRgb kColorReticule = qRgb(0, 255, 0);
constexpr QRgb kColorMax = qRgb(255, 0, 0);
constexpr QRgb kColorMin = qRgb(0, 0, 255);

double percentileSorted(const std::vector<std::uint16_t>& sorted, double p)
{
    if (sorted.empty()) {
        return 0.0;
    }
    const double rank = (p / 100.0) * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(rank));
    const auto upper = static_cast<std::size_t>(std::ceil(rank));
    const double fraction = rank - static_cast<double>(lower);
    return static_cast<double>(sorted[lower]) * (1.0 - fraction) +
           static_cast<double>(sorted[upper]) * fraction;
}

void sortedValues(const Image16& image, std::vector<std::uint16_t>& out)
{
    out = image.data;
    std::sort(out.begin(), out.end());
}

float percentileSortedFloat(std::vector<float>& sorted, double p)
{
    if (sorted.empty()) {
        return 0.0F;
    }
    const double rank = (p / 100.0) * static_cast<double>(sorted.size() - 1);
    const auto lower = static_cast<std::size_t>(std::floor(rank));
    const auto upper = static_cast<std::size_t>(std::ceil(rank));
    const double fraction = rank - static_cast<double>(lower);
    return static_cast<float>(static_cast<double>(sorted[lower]) * (1.0 - fraction) +
                              static_cast<double>(sorted[upper]) * fraction);
}

Image8 agcTemporal(const Image16& thermal, double pct, double alpha, bool& initialized, double& emaLow,
                   double& emaHigh)
{
    Image8 out;
    std::vector<std::uint16_t> values;
    sortedValues(thermal, values);
    const double low = percentileSorted(values, pct);
    const double high = percentileSorted(values, 100.0 - pct);

    if (!initialized) {
        emaLow = low;
        emaHigh = high;
        initialized = true;
    } else {
        emaLow = alpha * low + (1.0 - alpha) * emaLow;
        emaHigh = alpha * high + (1.0 - alpha) * emaHigh;
    }

    out.resize(thermal.width, thermal.height);
    if (emaHigh <= emaLow) {
        std::fill(out.data.begin(), out.data.end(), static_cast<std::uint8_t>(0));
        return out;
    }

    const double scale = 255.0 / (emaHigh - emaLow);
    for (std::size_t i = 0; i < thermal.data.size(); ++i) {
        const double normalized = (static_cast<double>(thermal.data[i]) - emaLow) * scale;
        out.data[i] = static_cast<std::uint8_t>(std::lround(std::clamp(normalized, 0.0, 255.0)));
    }
    return out;
}

Image8 agcFixed(const Image16& thermal, double tempMinC, double tempMaxC)
{
    Image8 out;
    out.resize(thermal.width, thermal.height);
    const double rawMin = celsiusToRaw(tempMinC);
    const double rawMax = celsiusToRaw(tempMaxC);
    if (rawMax <= rawMin) {
        std::fill(out.data.begin(), out.data.end(), static_cast<std::uint8_t>(0));
        return out;
    }
    const double scale = 255.0 / (rawMax - rawMin);
    for (std::size_t i = 0; i < thermal.data.size(); ++i) {
        const double normalized = (static_cast<double>(thermal.data[i]) - rawMin) * scale;
        out.data[i] = static_cast<std::uint8_t>(std::lround(std::clamp(normalized, 0.0, 255.0)));
    }
    return out;
}

Image8 gaussianBlur3x3(const Image8& in)
{
    Image8 out;
    out.resize(in.width, in.height);
    const auto sample = [&in](int x, int y) -> int {
        const int cx = std::clamp(x, 0, in.width - 1);
        const int cy = std::clamp(y, 0, in.height - 1);
        return in.at(cx, cy);
    };
    for (int y = 0; y < in.height; ++y) {
        for (int x = 0; x < in.width; ++x) {
            const int value = sample(x - 1, y - 1) + 2 * sample(x, y - 1) + sample(x + 1, y - 1) +
                              2 * sample(x - 1, y) + 4 * sample(x, y) + 2 * sample(x + 1, y) +
                              sample(x - 1, y + 1) + 2 * sample(x, y + 1) + sample(x + 1, y + 1);
            out.at(x, y) = static_cast<std::uint8_t>((value + 8) / 16);
        }
    }
    return out;
}

Image8 dde(const Image8& in, double strength)
{
    if (strength <= 0.0) {
        return in;
    }
    const Image8 blurred = gaussianBlur3x3(in);
    Image8 out;
    out.resize(in.width, in.height);
    for (std::size_t i = 0; i < in.data.size(); ++i) {
        const double original = in.data[i];
        const double enhanced = original + strength * (original - static_cast<double>(blurred.data[i]));
        out.data[i] = static_cast<std::uint8_t>(std::lround(std::clamp(enhanced, 0.0, 255.0)));
    }
    return out;
}

struct TileLut {
    std::array<std::uint8_t, kColormapSize> map{};
};

Image8 clahe(const Image8& in, double clipLimit, int tilesX = 8, int tilesY = 8)
{
    if (in.isEmpty()) {
        return in;
    }

    const int tileW = std::max(1, (in.width + tilesX - 1) / tilesX);
    const int tileH = std::max(1, (in.height + tilesY - 1) / tilesY);

    std::vector<TileLut> tiles(static_cast<std::size_t>(tilesX) * tilesY);
    for (int by = 0; by < tilesY; ++by) {
        for (int bx = 0; bx < tilesX; ++bx) {
            const int x0 = bx * tileW;
            const int y0 = by * tileH;
            const int x1 = std::min(in.width, x0 + tileW);
            const int y1 = std::min(in.height, y0 + tileH);

            std::array<int, kColormapSize> histogram{};
            int pixelCount = 0;
            for (int y = y0; y < y1; ++y) {
                for (int x = x0; x < x1; ++x) {
                    ++histogram[in.at(x, y)];
                    ++pixelCount;
                }
            }
            if (pixelCount == 0) {
                ++pixelCount;
            }

            const int threshold = std::max(1, static_cast<int>(clipLimit * pixelCount / kColormapSize));
            int excess = 0;
            for (int& bin : histogram) {
                if (bin > threshold) {
                    excess += bin - threshold;
                    bin = threshold;
                }
            }
            const int increment = excess / kColormapSize;
            const int remainder = excess % kColormapSize;
            for (int i = 0; i < kColormapSize; ++i) {
                histogram[i] += increment;
                if (i < remainder) {
                    ++histogram[i];
                }
            }

            int cumulative = 0;
            int minimum = 0;
            for (int i = 0; i < kColormapSize; ++i) {
                cumulative += histogram[i];
                if (histogram[i] != 0 && minimum == 0) {
                    minimum = cumulative;
                }
                const double denominator = static_cast<double>(pixelCount - minimum);
                const double value =
                    denominator > 0.0 ? (cumulative - minimum) / denominator * 255.0 : 0.0;
                tiles[static_cast<std::size_t>(by) * tilesX + bx].map[i] =
                    static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0, 255.0)));
            }
        }
    }

    const auto blend = [](int index, int count, int size) {
        double f = (static_cast<double>(index) + 0.5) * count / size - 0.5;
        int i0 = static_cast<int>(std::floor(f));
        double weight = f - i0;
        if (i0 < 0) {
            i0 = 0;
            weight = 0.0;
        }
        if (i0 >= count - 1) {
            i0 = count - 1;
            weight = 0.0;
        }
        return std::array<double, 3>{static_cast<double>(i0),
                                     static_cast<double>(std::min(i0 + 1, count - 1)), weight};
    };

    Image8 out;
    out.resize(in.width, in.height);
    for (int y = 0; y < in.height; ++y) {
        const auto [ty0, ty1, wy] = blend(y, tilesY, in.height);
        for (int x = 0; x < in.width; ++x) {
            const auto [tx0, tx1, wx] = blend(x, tilesX, in.width);
            const int value = in.at(x, y);
            const int i00 = tiles[static_cast<std::size_t>(ty0) * tilesX + static_cast<int>(tx0)].map[value];
            const int i01 = tiles[static_cast<std::size_t>(ty0) * tilesX + static_cast<int>(tx1)].map[value];
            const int i10 = tiles[static_cast<std::size_t>(ty1) * tilesX + static_cast<int>(tx0)].map[value];
            const int i11 = tiles[static_cast<std::size_t>(ty1) * tilesX + static_cast<int>(tx1)].map[value];
            const double top = i00 * (1.0 - wx) + i01 * wx;
            const double bottom = i10 * (1.0 - wx) + i11 * wx;
            out.at(x, y) = static_cast<std::uint8_t>(std::lround(std::clamp(top * (1.0 - wy) + bottom * wy, 0.0, 255.0)));
        }
    }
    return out;
}

QImage toQImageGray(const Image8& image)
{
    if (image.isEmpty()) {
        return {};
    }
    QImage wrapped(image.data.data(), image.width, image.height, image.width, QImage::Format_Grayscale8);
    return wrapped.copy();
}

Image8 scaleGray(const Image8& image, ScaleMode mode)
{
    if (mode == ScaleMode::Off || image.isEmpty()) {
        return image;
    }
    const QImage source = toQImageGray(image);
    const Qt::TransformationMode transform =
        mode == ScaleMode::Nearest ? Qt::FastTransformation : Qt::SmoothTransformation;
    const QImage scaled = source.scaled(image.width * 2, image.height * 2, Qt::IgnoreAspectRatio, transform);
    Image8 out;
    out.resize(scaled.width(), scaled.height());
    for (int y = 0; y < scaled.height(); ++y) {
        std::memcpy(out.row(y), scaled.constScanLine(y), static_cast<std::size_t>(scaled.width()));
    }
    return out;
}

Image8 flipHorizontal(const Image8& image)
{
    Image8 out;
    out.resize(image.width, image.height);
    for (int y = 0; y < image.height; ++y) {
        const std::uint8_t* source = image.row(y);
        std::uint8_t* target = out.row(y);
        for (int x = 0; x < image.width; ++x) {
            target[x] = source[image.width - 1 - x];
        }
    }
    return out;
}

QImage rotateImage(const QImage& image, int degrees)
{
    const int normalized = ((degrees % 360) + 360) % 360;
    if (normalized == 0) {
        return image;
    }
    QTransform transform;
    transform.rotate(normalized);
    return image.transformed(transform, Qt::FastTransformation);
}

void drawBoxMarker(QPainter& painter, const QPoint& center, const QString& annotation, QRgb color,
                   int size)
{
    const int half = size / 2;
    painter.setPen(QColor::fromRgb(color));
    painter.drawRect(center.x() - half, center.y() - half, size, size);
    QFont font = painter.font();
    font.setPixelSize(std::max(10, size));
    painter.setFont(font);
    const int yOffset = center.y() < size + font.pixelSize() ? size + font.pixelSize()
                                                            : -size - font.pixelSize() / 2;
    painter.drawText(QPoint(center.x() - half, center.y() + yOffset), annotation);
}

void drawColorbar(QPainter& painter, const QImage& image, const ProcessingParams& params, int rangeMin,
                  int rangeMax, double tempMin, double tempMax)
{
    const int imageWidth = image.width();
    const int imageHeight = image.height();
    const int reference = std::max(1, std::min(imageWidth, imageHeight));
    constexpr int tickCount = 5;

    QFont font = painter.font();
    font.setPixelSize(std::max(8, reference / 40));
    painter.setFont(font);
    const QFontMetrics metrics(font);

    QStringList labels;
    int labelWidth = 0;
    for (int i = 0; i < tickCount; ++i) {
        const double fraction = static_cast<double>(i) / (tickCount - 1);
        const double value = tempMin + fraction * (tempMax - tempMin);
        const QString text = QString::number(value, 'f', 1);
        labels.append(text);
        labelWidth = std::max(labelWidth, metrics.horizontalAdvance(text));
    }

    const int padding = std::max(4, reference / 100);
    const int barWidth = std::max(4, reference / 40);
    const int barHeight = std::max(2, imageHeight / 2);
    const int barX = std::max(0, imageWidth - (barWidth + padding + labelWidth + padding));
    const int barY = (imageHeight - barHeight) / 2;

    const double valueRange = std::max(1, rangeMax - rangeMin);
    const auto& lut = colormapLut(params.colormap);

    QImage strip(barWidth, barHeight, QImage::Format_RGB32);
    for (int row = 0; row < barHeight; ++row) {
        const double fraction = 1.0 - static_cast<double>(row) / std::max(1, barHeight - 1);
        const int index = std::clamp(static_cast<int>(rangeMin + fraction * valueRange), 0, 255);
        auto* line = reinterpret_cast<QRgb*>(strip.scanLine(row));
        for (int column = 0; column < barWidth; ++column) {
            line[column] = lut[static_cast<std::size_t>(index)];
        }
    }
    painter.drawImage(barX, barY, strip);

    painter.setPen(QColor::fromRgb(kColorText));
    painter.drawRect(barX, barY, barWidth, barHeight);

    const int textX = barX + barWidth + padding;
    for (int i = 0; i < tickCount; ++i) {
        const double fraction = static_cast<double>(i) / (tickCount - 1);
        const int tickY = barY + static_cast<int>((1.0 - fraction) * (barHeight - 1));
        painter.drawLine(barX, tickY, barX + barWidth, tickY);
        painter.drawText(QPoint(textX, tickY + metrics.ascent() / 2), labels.at(i));
    }
}

QPoint coordToImage(int x, int y, const Image16& thermal, const QImage& image,
                    const ProcessingParams& params)
{
    int h = image.height();
    int w = image.width();
    if (params.rotation == 90 || params.rotation == 270) {
        std::swap(h, w);
    }

    int cx = static_cast<int>(std::lround(static_cast<double>(x) / thermal.width * w));
    int cy = static_cast<int>(std::lround(static_cast<double>(y) / thermal.height * h));

    if (params.mirror) {
        cx = w - cx - 1;
    }

    if (params.rotation == 90) {
        const int newCy = cx;
        const int newCx = h - cy - 1;
        cy = newCy;
        cx = newCx;
    } else if (params.rotation == 180) {
        const int newCy = h - cy - 1;
        const int newCx = w - cx - 1;
        cy = newCy;
        cx = newCx;
    } else if (params.rotation == 270) {
        const int newCy = w - cx - 1;
        const int newCx = cy;
        cy = newCy;
        cx = newCx;
    }

    return QPoint(cx, cy);
}

void drawOverlays(QImage& image, const Image16& thermal, const ProcessingParams& params,
                  const ProcessedFrame& frame, int rangeMin, int rangeMax)
{
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QFont font = painter.font();
    font.setPixelSize(std::max(8, std::min(image.width(), image.height()) / 40));
    painter.setFont(font);
    painter.setPen(QColor::fromRgb(kColorText));

    const QString top = QStringLiteral("Spot: %1C | Range: %2-%3C")
                            .arg(frame.spotTemp, 0, 'f', 1)
                            .arg(frame.minTemp, 0, 'f', 1)
                            .arg(frame.maxTemp, 0, 'f', 1);
    painter.drawText(QPoint(10, font.pixelSize() + 4), top);

    const QString gain = params.gain == GainMode::Low ? QStringLiteral("LOW") : QStringLiteral("HIGH");
    const QString scale = params.scaleMode == ScaleMode::Off ? QString() : QStringLiteral(" 2x");
    const QString bottom = QStringLiteral("%1 FPS | %2 | %3 | e=%4%5")
                               .arg(params.fps, 0, 'f', 1)
                               .arg(colormapName(params.colormap))
                               .arg(gain)
                               .arg(params.env.emissivity, 0, 'f', 2)
                               .arg(scale);
    painter.drawText(QPoint(10, image.height() - 10), bottom);

    if (params.showColorbar) {
        drawColorbar(painter, image, params, rangeMin, rangeMax, frame.minTemp, frame.maxTemp);
    }

    if (params.showReticule) {
        const int cx = image.width() / 2;
        const int cy = image.height() / 2;
        painter.setPen(QColor::fromRgb(kColorReticule));
        painter.drawLine(cx - 15, cy, cx + 15, cy);
        painter.drawLine(cx, cy - 15, cx, cy + 15);
        painter.drawText(QPoint(cx + 20, cy - 5), QStringLiteral("%1C").arg(frame.spotTemp, 0, 'f', 1));
    }

    const int markerSize = std::max(16, image.height() / 24);
    if (params.hotspot == HotspotMode::Max || params.hotspot == HotspotMode::MinMax) {
        const QPoint point = coordToImage(frame.hotX, frame.hotY, thermal, image, params);
        drawBoxMarker(painter, point, QStringLiteral("%1C").arg(frame.maxTemp, 0, 'f', 1), kColorMax,
                      markerSize);
    }
    if (params.hotspot == HotspotMode::Min || params.hotspot == HotspotMode::MinMax) {
        const QPoint point = coordToImage(frame.coldX, frame.coldY, thermal, image, params);
        drawBoxMarker(painter, point, QStringLiteral("%1C").arg(frame.minTemp, 0, 'f', 1), kColorMin,
                      markerSize);
    }

    if (params.showHelp) {
        static const QStringList lines = {
            QStringLiteral("Rotation / mirror / zoom: View menu"),
            QStringLiteral("Colormap / colorbar / hotspots: View menu"),
            QStringLiteral("Shutter / gain: Camera menu"),
            QStringLiteral("AGC / enhancement / emissivity: Processing menu"),
            QStringLiteral("Lock-in thermography: Lock-In menu"),
            QStringLiteral("Screenshot / raw dump: File menu"),
            QStringLiteral("User guide: Help menu (F1)"),
        };
        const int lineHeight = font.pixelSize() + 6;
        const int boxWidth = image.width() / 2;
        const int boxHeight = lineHeight * lines.size() + 16;
        QColor background(0, 0, 0, 180);
        painter.fillRect(QRect(5, 30, boxWidth, boxHeight), background);
        painter.setPen(QColor::fromRgb(kColorText));
        for (int i = 0; i < lines.size(); ++i) {
            painter.drawText(QPoint(12, 30 + 12 + lineHeight * (i + 1) - 8), lines.at(i));
        }
    }

    painter.end();
}

QRgb divergingColor(double value)
{
    const double t = std::clamp(value, -1.0, 1.0);
    if (t < 0.0) {
        const double u = -t;
        return qRgb(static_cast<int>(std::lround(255.0 * (1.0 - u))),
                    static_cast<int>(std::lround(255.0 * (1.0 - u))), 255);
    }
    return qRgb(255, static_cast<int>(std::lround(255.0 * (1.0 - t))),
                static_cast<int>(std::lround(255.0 * (1.0 - t))));
}

QImage divergingMap(const ImageFloat& data)
{
    if (data.isEmpty()) {
        return {};
    }
    std::vector<float> sorted = data.data;
    std::sort(sorted.begin(), sorted.end());
    float low = percentileSortedFloat(sorted, 0.01);
    float high = percentileSortedFloat(sorted, 99.99);
    if (high <= low) {
        high = low + 1.0F;
    }

    QImage image(data.width, data.height, QImage::Format_RGB32);
    for (int y = 0; y < data.height; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < data.width; ++x) {
            const double normalized = 2.0 * (static_cast<double>(data.at(x, y)) - low) / (high - low) - 1.0;
            scanLine[x] = divergingColor(normalized);
        }
    }
    return image;
}

QImage amplitudeMap(const ImageFloat& data, Colormap colormap)
{
    if (data.isEmpty()) {
        return {};
    }
    std::vector<float> sorted = data.data;
    std::sort(sorted.begin(), sorted.end());
    float low = percentileSortedFloat(sorted, 0.01);
    float high = percentileSortedFloat(sorted, 99.99);
    if (high <= low) {
        high = low + 1.0F;
    }

    Image8 gray;
    gray.resize(data.width, data.height);
    for (std::size_t i = 0; i < data.data.size(); ++i) {
        const double normalized = (static_cast<double>(data.data[i]) - low) / (high - low);
        gray.data[i] = static_cast<std::uint8_t>(std::lround(std::clamp(normalized, 0.0, 1.0) * 255.0));
    }
    return applyColormap(gray, colormap);
}

QImage angleMap(const ImageFloat& angle, const ImageFloat& amplitude)
{
    if (angle.isEmpty() || amplitude.isEmpty()) {
        return {};
    }
    double sum = 0.0;
    for (const float value : amplitude.data) {
        sum += value;
    }
    const double mean = amplitude.data.empty() ? 0.0 : sum / static_cast<double>(amplitude.data.size());

    QImage image(angle.width, angle.height, QImage::Format_RGB32);
    for (int y = 0; y < angle.height; ++y) {
        auto* scanLine = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < angle.width; ++x) {
            if (amplitude.at(x, y) < mean) {
                scanLine[x] = qRgb(0, 0, 0);
                continue;
            }
            const double normalized = (angle.at(x, y) + kPi) / (2.0 * kPi);
            const QColor color = QColor::fromHsvF(std::clamp(normalized, 0.0, 0.9999), 1.0, 1.0);
            scanLine[x] = color.rgb();
        }
    }
    return image;
}

QImage compositeLockIn(const QImage& mainImage, const LockInPanes& panes, Colormap colormap)
{
    if (mainImage.isNull() || !panes.isValid()) {
        return mainImage;
    }

    const QImage inPhase = divergingMap(panes.inPhase);
    const QImage quadrature = divergingMap(panes.quadrature);
    const QImage amplitude = amplitudeMap(panes.amplitude, colormap);
    const QImage angle = angleMap(panes.angle, panes.amplitude);

    const int paneWidth = std::max(64, mainImage.width() / 3);
    const int paneHeight = std::max(1, mainImage.height() / 2);

    const auto resizePane = [paneWidth, paneHeight](const QImage& source) {
        return source.isNull() ? QImage(paneWidth, paneHeight, QImage::Format_RGB32)
                               : source.scaled(paneWidth, paneHeight, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation);
    };

    const QImage topLeft = resizePane(inPhase);
    const QImage topRight = resizePane(amplitude);
    const QImage bottomLeft = resizePane(quadrature);
    const QImage bottomRight = resizePane(angle);

    QImage result(mainImage.width() + 2 * paneWidth,
                  std::max(mainImage.height(), 2 * paneHeight), QImage::Format_RGB32);
    result.fill(Qt::black);

    QPainter painter(&result);
    painter.drawImage(0, 0, mainImage);
    painter.drawImage(mainImage.width(), 0, topLeft);
    painter.drawImage(mainImage.width() + paneWidth, 0, topRight);
    painter.drawImage(mainImage.width(), paneHeight, bottomLeft);
    painter.drawImage(mainImage.width() + paneWidth, paneHeight, bottomRight);
    painter.end();
    return result;
}

} // namespace

void ImageProcessor::resetTemporal()
{
    m_previousFrame = {};
    m_emaInitialized = false;
    m_emaLow = 0.0;
    m_emaHigh = 0.0;
}

ProcessedFrame ImageProcessor::process(const Image16& thermal, const Image8& ir,
                                       const ProcessingParams& params, const LockInPanes* lockIn)
{
    ProcessedFrame frame;
    if (thermal.isEmpty()) {
        return frame;
    }

    Image16 filtered = thermal;
    if (!m_previousFrame.isEmpty() && m_previousFrame.width == thermal.width &&
        m_previousFrame.height == thermal.height && params.tnrAlpha < 1.0) {
        filtered.resize(thermal.width, thermal.height);
        for (std::size_t i = 0; i < thermal.data.size(); ++i) {
            const double value = params.tnrAlpha * thermal.data[i] +
                                 (1.0 - params.tnrAlpha) * m_previousFrame.data[i];
            filtered.data[i] = static_cast<std::uint16_t>(std::lround(value));
        }
    }
    m_previousFrame = thermal;

    Image8 gray;
    switch (params.agcMode) {
    case AgcMode::Factory:
        if (!ir.isEmpty() && ir.width == thermal.width && ir.height == thermal.height) {
            gray = ir;
        } else {
            gray = agcTemporal(filtered, 1.0, 0.1, m_emaInitialized, m_emaLow, m_emaHigh);
        }
        break;
    case AgcMode::Fixed:
        gray = agcFixed(filtered, params.fixedRangeMin, params.fixedRangeMax);
        break;
    case AgcMode::Temporal:
        gray = agcTemporal(filtered, 1.0, 0.1, m_emaInitialized, m_emaLow, m_emaHigh);
        break;
    }

    gray = scaleGray(gray, params.scaleMode);
    if (params.useClahe) {
        gray = clahe(gray, 2.0);
    }
    if (params.ddeStrength > 0.0) {
        gray = dde(gray, params.ddeStrength);
    }

    int rangeMin = 255;
    int rangeMax = 0;
    for (const std::uint8_t value : gray.data) {
        rangeMin = std::min(rangeMin, static_cast<int>(value));
        rangeMax = std::max(rangeMax, static_cast<int>(value));
    }
    if (gray.data.empty()) {
        rangeMin = 0;
    }

    const int centerX = params.mirror ? thermal.width - 1 - thermal.width / 2 : thermal.width / 2;
    const int centerY = thermal.height / 2;
    frame.spotTemp = rawToCelsiusCorrected(thermal.at(centerX, centerY), params.env);

    int hotIndex = 0;
    int coldIndex = 0;
    std::uint16_t maxRaw = thermal.data[0];
    std::uint16_t minRaw = thermal.data[0];
    for (std::size_t i = 1; i < thermal.data.size(); ++i) {
        if (thermal.data[i] > maxRaw) {
            maxRaw = thermal.data[i];
            hotIndex = static_cast<int>(i);
        }
        if (thermal.data[i] < minRaw) {
            minRaw = thermal.data[i];
            coldIndex = static_cast<int>(i);
        }
    }
    frame.hotX = hotIndex % thermal.width;
    frame.hotY = hotIndex / thermal.width;
    frame.coldX = coldIndex % thermal.width;
    frame.coldY = coldIndex / thermal.width;
    frame.maxTemp = rawToCelsiusCorrected(maxRaw, params.env);
    frame.minTemp = rawToCelsiusCorrected(minRaw, params.env);

    if (params.mirror) {
        gray = flipHorizontal(gray);
    }

    QImage image = applyColormap(gray, params.colormap);
    image = rotateImage(image, params.rotation);

    const int zoom = std::clamp(params.zoom, 1, 8);
    if (zoom > 1) {
        image = image.scaled(image.width() * zoom, image.height() * zoom, Qt::IgnoreAspectRatio,
                             Qt::SmoothTransformation);
    }

    drawOverlays(image, thermal, params, frame, rangeMin, rangeMax);

    if (lockIn != nullptr && lockIn->isValid()) {
        image = compositeLockIn(image, *lockIn, params.colormap);
    }

    frame.image = image;
    return frame;
}

} // namespace qtthermal
