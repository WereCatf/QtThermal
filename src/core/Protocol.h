#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace qtthermal {

/// Size of a USB control command.
inline constexpr std::size_t kCommandSize = 18;

/// Pre-computed camera commands.
enum class Command {
    ReadName,
    ReadVersion,
    ReadPartNumber,
    ReadSerial,
    ReadHwVersion,
    ReadModelLong,
    Status,
    StartStream,
    GainLow,
    GainHigh,
    Shutter,
};

/// Compute a CRC16-CCITT checksum (poly 0x1021, init 0x0000).
[[nodiscard]] std::uint16_t crc16Ccitt(std::span<const std::uint8_t> data, std::uint16_t polynomial = 0x1021,
                                       std::uint16_t initial = 0x0000);

/// Build an 18-byte USB command with a trailing CRC16.
[[nodiscard]] std::array<std::uint8_t, kCommandSize> buildCommand(std::uint16_t cmdType,
                                                                  std::uint16_t param,
                                                                  std::uint16_t reg,
                                                                  std::uint16_t respLen);

/// Return the raw bytes of a pre-computed command.
[[nodiscard]] const std::array<std::uint8_t, kCommandSize>& commandBytes(Command command);

} // namespace qtthermal
