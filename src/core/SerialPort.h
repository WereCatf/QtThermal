#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace qtthermal {

/// Minimal cross-platform serial port used by the lock-in controller.
class SerialPort {
public:
    SerialPort() = default;
    ~SerialPort();

    SerialPort(const SerialPort&) = delete;
    SerialPort& operator=(const SerialPort&) = delete;

    /// Open a serial port (e.g. "COM3" or "/dev/ttyACM0").
    bool open(const std::string& port, int baudRate, std::string* error = nullptr);

    /// Close the port if open.
    void close();

    [[nodiscard]] bool isOpen() const;

    /// Write raw bytes to the port.
    bool write(std::string_view data, std::string* error = nullptr);

private:
#ifdef _WIN32
    void* m_handle = nullptr;
#else
    int m_fd = -1;
#endif
};

} // namespace qtthermal
