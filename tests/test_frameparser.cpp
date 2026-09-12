#include "core/FrameParser.h"

#include <QtTest>

#include <vector>

using namespace qtthermal;

namespace {

std::vector<std::uint8_t> makeFrame(const ModelConfig& config, std::uint16_t thermalValue,
                                    std::uint8_t irValue)
{
    std::vector<std::uint8_t> frame(static_cast<std::size_t>(kMarkerSize + config.frameSize()), 0);
    frame[0] = 0x0C;
    frame[1] = 0x8C;
    frame[2] = 0x01;

    for (int row = 0; row < config.frameRows(); ++row) {
        for (int column = 0; column < config.sensorWidth; ++column) {
            const std::size_t offset =
                static_cast<std::size_t>(kMarkerSize) +
                (static_cast<std::size_t>(row) * config.sensorWidth + column) * 2;
            std::uint16_t value = 0;
            if (row < config.irRowEnd()) {
                value = irValue;
            } else if (row >= config.thermalRowStart() && row < config.thermalRowEnd()) {
                value = thermalValue;
            }
            frame[offset] = static_cast<std::uint8_t>(value & 0xFF);
            frame[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
        }
    }
    return frame;
}

} // namespace

class FrameParserTest : public QObject {
    Q_OBJECT

private slots:
    void parseMarkerFields();
    void parseMarkerWrap();
    void extractThermalP3();
    void extractThermalP1();
    void extractTooShort();
    void extractIr();
    void extractBothImages();
};

void FrameParserTest::parseMarkerFields()
{
    const std::uint8_t bytes[12] = {0x0C, 0x8C, 0x01, 0x00, 0x00, 0x00,
                                    0x02, 0x00, 0x00, 0x00, 0x28, 0x00};
    const Marker marker = parseMarker(bytes);
    QCOMPARE(static_cast<int>(marker.length), 12);
    QCOMPARE(static_cast<int>(marker.sync), 0x8C);
    QCOMPARE(marker.cnt1, static_cast<std::uint32_t>(1));
    QCOMPARE(marker.cnt2, static_cast<std::uint32_t>(2));
    QCOMPARE(static_cast<int>(marker.cnt3), 40);
}

void FrameParserTest::parseMarkerWrap()
{
    const std::uint8_t bytes[12] = {0x0C, 0x8D, 0, 0, 0, 0, 0, 0, 0, 0, 0x00, 0x08};
    const Marker marker = parseMarker(bytes);
    QCOMPARE(marker.cnt3, static_cast<std::uint16_t>(kCnt3Wrap));
}

void FrameParserTest::extractThermalP3()
{
    const ModelConfig config = modelConfig(Model::P3);
    const auto frame = makeFrame(config, 20000, 0x80);
    Image16 thermal;
    QVERIFY(extractThermalData(frame, config, thermal));
    QCOMPARE(thermal.width, 256);
    QCOMPARE(thermal.height, 192);
    QCOMPARE(static_cast<int>(thermal.at(0, 0)), 20000);
}

void FrameParserTest::extractThermalP1()
{
    const ModelConfig config = modelConfig(Model::P1);
    const auto frame = makeFrame(config, 19000, 0x40);
    Image16 thermal;
    QVERIFY(extractThermalData(frame, config, thermal));
    QCOMPARE(thermal.width, 160);
    QCOMPARE(thermal.height, 120);
    QCOMPARE(static_cast<int>(thermal.at(10, 10)), 19000);
}

void FrameParserTest::extractTooShort()
{
    const ModelConfig config = modelConfig(Model::P3);
    const std::vector<std::uint8_t> frame(100, 0);
    Image16 thermal;
    QVERIFY(!extractThermalData(frame, config, thermal));
}

void FrameParserTest::extractIr()
{
    const ModelConfig config = modelConfig(Model::P3);
    const auto frame = makeFrame(config, 20000, 0xAB);
    Image8 ir;
    QVERIFY(extractIrBrightness(frame, config, ir));
    QCOMPARE(static_cast<int>(ir.at(0, 0)), 0xAB);
}

void FrameParserTest::extractBothImages()
{
    const ModelConfig config = modelConfig(Model::P3);
    const auto frame = makeFrame(config, 19000, 0x80);
    Image8 ir;
    Image16 thermal;
    QVERIFY(extractBoth(frame, config, ir, thermal));
    QCOMPARE(static_cast<int>(ir.at(0, 0)), 0x80);
    QCOMPARE(static_cast<int>(thermal.at(0, 0)), 19000);
}

QTEST_APPLESS_MAIN(FrameParserTest)

#include "test_frameparser.moc"
