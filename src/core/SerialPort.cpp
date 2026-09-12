#include "core/SerialPort.h"

#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace qtthermal {
namespace {

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

int baudToConstant(int baudRate)
{
    switch (baudRate) {
    case 9600:
        return 9600;
    case 19200:
        return 19200;
    case 38400:
        return 38400;
    case 57600:
        return 57600;
    case 115200:
        return 115200;
    case 230400:
        return 230400;
    default:
        return 115200;
    }
}

} // namespace

SerialPort::~SerialPort()
{
    close();
}

bool SerialPort::open(const std::string& port, int baudRate, std::string* error)
{
    close();

#ifdef _WIN32
    std::string device = port;
    if (device.rfind("\\\\.\\", 0) != 0) {
        device = "\\\\.\\" + device;
    }

    HANDLE handle = CreateFileA(device.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        setError(error, "Failed to open serial port " + port);
        return false;
    }

    DCB dcb{};
    dcb.DCBlength = sizeof(DCB);
    if (!GetCommState(handle, &dcb)) {
        setError(error, "Failed to query serial port state");
        CloseHandle(handle);
        return false;
    }
    dcb.BaudRate = static_cast<DWORD>(baudToConstant(baudRate));
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fParity = FALSE;
    if (!SetCommState(handle, &dcb)) {
        setError(error, "Failed to configure serial port");
        CloseHandle(handle);
        return false;
    }

    COMMTIMEOUTS timeouts{};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 100;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(handle, &timeouts);

    m_handle = handle;
    return true;
#else
    const int fd = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        setError(error, "Failed to open serial port " + port);
        return false;
    }

    termios options{};
    if (tcgetattr(fd, &options) != 0) {
        setError(error, "Failed to query serial port state");
        ::close(fd);
        return false;
    }

    cfmakeraw(&options);
    const speed_t speed = static_cast<speed_t>(baudToConstant(baudRate));
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);
    options.c_cflag |= CLOCAL | CREAD;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CRTSCTS;
    options.c_cc[VMIN] = 0;
    options.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        setError(error, "Failed to configure serial port");
        ::close(fd);
        return false;
    }

    m_fd = fd;
    return true;
#endif
}

void SerialPort::close()
{
#ifdef _WIN32
    if (m_handle != nullptr) {
        CloseHandle(static_cast<HANDLE>(m_handle));
        m_handle = nullptr;
    }
#else
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
#endif
}

bool SerialPort::isOpen() const
{
#ifdef _WIN32
    return m_handle != nullptr;
#else
    return m_fd >= 0;
#endif
}

bool SerialPort::write(std::string_view data, std::string* error)
{
    if (!isOpen()) {
        setError(error, "Serial port is not open");
        return false;
    }

#ifdef _WIN32
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(m_handle), data.data(), static_cast<DWORD>(data.size()), &written,
                   nullptr) ||
        written != data.size()) {
        setError(error, "Failed to write to serial port");
        return false;
    }
    return true;
#else
    const ssize_t written = ::write(m_fd, data.data(), data.size());
    if (written != static_cast<ssize_t>(data.size())) {
        setError(error, "Failed to write to serial port");
        return false;
    }
    return true;
#endif
}

} // namespace qtthermal
