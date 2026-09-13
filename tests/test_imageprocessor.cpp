#include "core/Colormap.h"
#include "core/ImageProcessor.h"
#include "core/Temperature.h"

#include <QtTest>

using namespace qtthermal;

namespace {

Image16 makeThermal(int width, int height)
{
    Image16 image;
    image.resize(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const double temperature = 20.0 + 10.0 * static_cast<double>(x) / width;
            image.at(x, y) = static_cast<std::uint16_t>(celsiusToRaw(temperature));
        }
    }
    return image;
}

Image8 makeIr(int width, int height)
{
    Image8 image;
    image.resize(width, height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            image.at(x, y) = static_cast<std::uint8_t>(x * 255 / width);
        }
    }
    return image;
}

ProcessingParams baseParams()
{
    ProcessingParams params;
    params.zoom = 1;
    params.scaleMode = ScaleMode::Off;
    params.useClahe = false;
    params.ddeStrength = 0.0;
    params.tnrAlpha = 1.0;
    params.showColorbar = false;
    params.showReticule = false;
    params.showHelp = false;
    return params;
}

double meanBrightness(const QImage& image)
{
    if (image.isNull()) {
        return 0.0;
    }
    qint64 sum = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = image.pixel(x, y);
            sum += qRed(pixel) + qGreen(pixel) + qBlue(pixel);
        }
    }
    return static_cast<double>(sum) / (static_cast<double>(image.width()) * image.height() * 3.0);
}

} // namespace

class ImageProcessorTest : public QObject {
    Q_OBJECT

private slots:
    void colormapDimensions();
    void colormapWhiteHotEndpoints();
    void processBasic();
    void processScaleDoublesSize();
    void processFixedRange();
    void processFixedRangeContainsScene();
    void processRotationSwapsAxes();
    void processWithOverlays();
};

void ImageProcessorTest::colormapDimensions()
{
    const Image8 gray = makeIr(32, 16);
    const QImage image = applyColormap(gray, Colormap::Ironbow);
    QCOMPARE(image.width(), 32);
    QCOMPARE(image.height(), 16);
}

void ImageProcessorTest::colormapWhiteHotEndpoints()
{
    Image8 gray;
    gray.resize(2, 1);
    gray.at(0, 0) = 0;
    gray.at(1, 0) = 255;
    const QImage image = applyColormap(gray, Colormap::WhiteHot);
    QCOMPARE(image.pixel(0, 0), qRgb(0, 0, 0));
    QCOMPARE(image.pixel(1, 0), qRgb(255, 255, 255));
}

void ImageProcessorTest::processBasic()
{
    ImageProcessor processor;
    const ProcessedFrame frame = processor.process(makeThermal(64, 48), makeIr(64, 48), baseParams());
    QVERIFY(!frame.image.isNull());
    QCOMPARE(frame.image.width(), 64);
    QCOMPARE(frame.image.height(), 48);
    QVERIFY(frame.spotTemp > 20.0);
    QVERIFY(frame.spotTemp < 30.0);
}

void ImageProcessorTest::processScaleDoublesSize()
{
    ImageProcessor processor;
    ProcessingParams params = baseParams();
    params.scaleMode = ScaleMode::Bilinear;
    const ProcessedFrame frame = processor.process(makeThermal(64, 48), makeIr(64, 48), params);
    QCOMPARE(frame.image.width(), 128);
    QCOMPARE(frame.image.height(), 96);
}

void ImageProcessorTest::processFixedRange()
{
    ImageProcessor processor;
    ProcessingParams params = baseParams();
    params.agcMode = AgcMode::Fixed;
    params.fixedRangeMin = 15.0;
    params.fixedRangeMax = 40.0;
    const ProcessedFrame frame = processor.process(makeThermal(64, 48), makeIr(64, 48), params);
    QVERIFY(!frame.image.isNull());
}

void ImageProcessorTest::processFixedRangeContainsScene()
{
    // The synthetic scene spans 20-30 C. A window that contains it must render
    // a visible image, while one entirely above it saturates to black.
    ImageProcessor processor;
    ProcessingParams inRange = baseParams();
    inRange.agcMode = AgcMode::Fixed;
    inRange.fixedRangeMin = 15.0;
    inRange.fixedRangeMax = 45.0;

    ProcessingParams aboveRange = inRange;
    aboveRange.fixedRangeMin = 100.0;
    aboveRange.fixedRangeMax = 200.0;

    const ProcessedFrame inFrame = processor.process(makeThermal(64, 48), makeIr(64, 48), inRange);
    const ProcessedFrame aboveFrame =
        processor.process(makeThermal(64, 48), makeIr(64, 48), aboveRange);

    QVERIFY(meanBrightness(inFrame.image) > meanBrightness(aboveFrame.image));
    QVERIFY(meanBrightness(inFrame.image) > 10.0);
}

void ImageProcessorTest::processRotationSwapsAxes()
{
    ImageProcessor processor;
    ProcessingParams params = baseParams();
    params.rotation = 90;
    const ProcessedFrame frame = processor.process(makeThermal(64, 48), makeIr(64, 48), params);
    QCOMPARE(frame.image.width(), 48);
    QCOMPARE(frame.image.height(), 64);
}

void ImageProcessorTest::processWithOverlays()
{
    ImageProcessor processor;
    ProcessingParams params = baseParams();
    params.showColorbar = true;
    params.showReticule = true;
    params.showHelp = true;
    params.useClahe = true;
    params.ddeStrength = 0.3;
    const ProcessedFrame frame = processor.process(makeThermal(96, 72), makeIr(96, 72), params);
    QVERIFY(!frame.image.isNull());
}

QTEST_MAIN(ImageProcessorTest)

#include "test_imageprocessor.moc"
