#include "core/Protocol.h"

#include <QtTest>

using namespace qtthermal;

class ProtocolTest : public QObject {
    Q_OBJECT

private slots:
    void crcKnownValue();
    void buildCommandLength();
    void buildCommandStructure();
    void precomputedCommandsMatchBuilder();
    void commandLengths();
};

void ProtocolTest::crcKnownValue()
{
    const std::array<std::uint8_t, 16> payload = {
        0x01, 0x01, 0x81, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x1e, 0x00, 0x00, 0x00,
    };
    QCOMPARE(crc16Ccitt(payload), static_cast<std::uint16_t>(0x904F));
}

void ProtocolTest::buildCommandLength()
{
    const auto command = buildCommand(0x0101, 0x0081, 0x0001, 30);
    QCOMPARE(command.size(), kCommandSize);
}

void ProtocolTest::buildCommandStructure()
{
    const auto command = buildCommand(0x0101, 0x0081, 0x0006, 64);
    QCOMPARE(command[0], static_cast<std::uint8_t>(0x01));
    QCOMPARE(command[1], static_cast<std::uint8_t>(0x01));
    QCOMPARE(command[2], static_cast<std::uint8_t>(0x81));
    QCOMPARE(command[3], static_cast<std::uint8_t>(0x00));
    QCOMPARE(command[4], static_cast<std::uint8_t>(0x06));
    QCOMPARE(command[5], static_cast<std::uint8_t>(0x00));
    QCOMPARE(command[12], static_cast<std::uint8_t>(0x40));
    QCOMPARE(command[13], static_cast<std::uint8_t>(0x00));
}

void ProtocolTest::precomputedCommandsMatchBuilder()
{
    const auto expected = buildCommand(0x0101, 0x0081, 0x0001, 30);
    const auto& actual = commandBytes(Command::ReadName);
    QVERIFY(actual == expected);
}

void ProtocolTest::commandLengths()
{
    const Command commands[] = {
        Command::ReadName,      Command::ReadVersion, Command::ReadPartNumber,
        Command::ReadSerial,    Command::ReadHwVersion, Command::ReadModelLong,
        Command::Status,        Command::StartStream, Command::GainLow,
        Command::GainHigh,      Command::Shutter,
    };
    for (const Command command : commands) {
        QCOMPARE(commandBytes(command).size(), kCommandSize);
    }
}

QTEST_APPLESS_MAIN(ProtocolTest)

#include "test_protocol.moc"
