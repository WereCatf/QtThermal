#include "core/Protocol.h"

#include <stdexcept>

namespace qtthermal {
namespace {

constexpr int hexValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

constexpr std::array<std::uint8_t, kCommandSize> commandFromHex(const char* hex)
{
    std::array<std::uint8_t, kCommandSize> result{};
    for (std::size_t i = 0; i < kCommandSize; ++i) {
        const int high = hexValue(hex[2 * i]);
        const int low = hexValue(hex[2 * i + 1]);
        if (high < 0 || low < 0) {
            throw std::logic_error("invalid command literal");
        }
        result[i] = static_cast<std::uint8_t>((high << 4) | low);
    }
    return result;
}

} // namespace

std::uint16_t crc16Ccitt(std::span<const std::uint8_t> data, std::uint16_t polynomial, std::uint16_t initial)
{
    std::uint16_t crc = initial;
    for (const std::uint8_t byte : data) {
        crc ^= static_cast<std::uint16_t>(byte) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            if ((crc & 0x8000) != 0) {
                crc = static_cast<std::uint16_t>((crc << 1) ^ polynomial);
            } else {
                crc = static_cast<std::uint16_t>(crc << 1);
            }
        }
    }
    return crc;
}

std::array<std::uint8_t, kCommandSize> buildCommand(std::uint16_t cmdType, std::uint16_t param,
                                                   std::uint16_t reg, std::uint16_t respLen)
{
    std::array<std::uint8_t, kCommandSize> command{};
    command[0] = static_cast<std::uint8_t>(cmdType & 0xFF);
    command[1] = static_cast<std::uint8_t>((cmdType >> 8) & 0xFF);
    command[2] = static_cast<std::uint8_t>(param & 0xFF);
    command[3] = static_cast<std::uint8_t>((param >> 8) & 0xFF);
    command[4] = static_cast<std::uint8_t>(reg & 0xFF);
    command[5] = static_cast<std::uint8_t>((reg >> 8) & 0xFF);
    command[12] = static_cast<std::uint8_t>(respLen & 0xFF);
    command[13] = static_cast<std::uint8_t>((respLen >> 8) & 0xFF);

    const std::uint16_t crc = crc16Ccitt(std::span<const std::uint8_t>(command.data(), 16));
    command[16] = static_cast<std::uint8_t>(crc & 0xFF);
    command[17] = static_cast<std::uint8_t>((crc >> 8) & 0xFF);
    return command;
}

const std::array<std::uint8_t, kCommandSize>& commandBytes(Command command)
{
    static const std::array<std::uint8_t, kCommandSize> readName =
        commandFromHex("0101810001000000000000001e0000004f90");
    static const std::array<std::uint8_t, kCommandSize> readVersion =
        commandFromHex("0101810002000000000000000c0000001f63");
    static const std::array<std::uint8_t, kCommandSize> readPartNumber =
        commandFromHex("01018100060000000000000040000000654f");
    static const std::array<std::uint8_t, kCommandSize> readSerial =
        commandFromHex("01018100070000000000000040000000104c");
    static const std::array<std::uint8_t, kCommandSize> readHwVersion =
        commandFromHex("010181000a00000000000000400000001959");
    static const std::array<std::uint8_t, kCommandSize> readModelLong =
        commandFromHex("010181000f0000000000000040000000b857");
    static const std::array<std::uint8_t, kCommandSize> status =
        commandFromHex("1021810000000000000000000200000095d1");
    static const std::array<std::uint8_t, kCommandSize> startStream =
        commandFromHex("012f81000000000000000000010000004930");
    static const std::array<std::uint8_t, kCommandSize> gainLow =
        commandFromHex("012f41000000000000000000000000003c3a");
    static const std::array<std::uint8_t, kCommandSize> gainHigh =
        commandFromHex("012f41000100000000000000000000004939");
    static const std::array<std::uint8_t, kCommandSize> shutter =
        commandFromHex("01364300000000000000000000000000cd0b");

    switch (command) {
    case Command::ReadName:
        return readName;
    case Command::ReadVersion:
        return readVersion;
    case Command::ReadPartNumber:
        return readPartNumber;
    case Command::ReadSerial:
        return readSerial;
    case Command::ReadHwVersion:
        return readHwVersion;
    case Command::ReadModelLong:
        return readModelLong;
    case Command::Status:
        return status;
    case Command::StartStream:
        return startStream;
    case Command::GainLow:
        return gainLow;
    case Command::GainHigh:
        return gainHigh;
    case Command::Shutter:
        return shutter;
    }
    return readName;
}

} // namespace qtthermal
