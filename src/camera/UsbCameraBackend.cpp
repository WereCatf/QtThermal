#include "camera/UsbCameraBackend.h"

#include "core/FrameParser.h"

#include <array>
#include <chrono>
#include <cstring>
#include <span>
#include <thread>

#ifdef _WIN32
#include <windows.h>

#include <setupapi.h>
#include <winusb.h>
#else
#if __has_include(<libusb-1.0/libusb.h>)
#include <libusb-1.0/libusb.h>
#else
#include <libusb.h>
#endif
#endif

namespace qtthermal {
namespace {

constexpr std::uint8_t kEndpointIn = 0x81;
constexpr int kControlTimeoutMs = 1000;
constexpr int kBulkTimeoutMs = 10000;

void setError(std::string* error, const std::string& message)
{
    if (error != nullptr) {
        *error = message;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Platform transports
// ---------------------------------------------------------------------------
#ifdef _WIN32

struct UsbCameraBackend::Impl {
    static constexpr GUID kWinUsbGuid = {0xDEE824EF,
                                         0x729B,
                                         0x4A0E,
                                         {0x9C, 0x14, 0xB7, 0x11, 0x7D, 0x33, 0xA8, 0x17}};
    static constexpr GUID kUsbDeviceGuid = {0xA5DCBF10,
                                            0x6530,
                                            0x11D2,
                                            {0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED}};

    HANDLE file0 = nullptr;
    HANDLE file1 = nullptr;
    WINUSB_INTERFACE_HANDLE interface0 = nullptr;
    WINUSB_INTERFACE_HANDLE interface1 = nullptr;
    bool interface1Associated = false;

    static bool matchesHardwareId(HDEVINFO deviceInfo, SP_DEVINFO_DATA* devInfoData,
                                  std::uint16_t pid)
    {
        wchar_t prefix[64] = {};
        swprintf(prefix, 64, L"USB\\VID_%04X&PID_%04X", kUsbVendorId, pid);

        DWORD type = 0;
        DWORD size = 0;
        SetupDiGetDeviceRegistryPropertyW(deviceInfo, devInfoData, SPDRP_HARDWAREID, &type, nullptr, 0,
                                          &size);
        if (size == 0) {
            return false;
        }
        std::vector<wchar_t> buffer(size / sizeof(wchar_t) + 1, L'\0');
        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfo, devInfoData, SPDRP_HARDWAREID, &type,
                                               reinterpret_cast<PBYTE>(buffer.data()), size, nullptr)) {
            return false;
        }

        const wchar_t* entry = buffer.data();
        while (*entry != L'\0') {
            if (_wcsnicmp(entry, prefix, wcslen(prefix)) == 0) {
                return true;
            }
            entry += wcslen(entry) + 1;
        }
        return false;
    }

    bool open(std::uint16_t pid, std::string* error)
    {
        const GUID guids[2] = {kWinUsbGuid, kUsbDeviceGuid};
        for (const GUID& guid : guids) {
            HDEVINFO deviceInfo = SetupDiGetClassDevsW(&guid, nullptr, nullptr,
                                                       DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
            if (deviceInfo == INVALID_HANDLE_VALUE) {
                continue;
            }

            SP_DEVICE_INTERFACE_DATA interfaceData{};
            interfaceData.cbSize = sizeof(interfaceData);
            for (DWORD index = 0;
                 SetupDiEnumDeviceInterfaces(deviceInfo, nullptr, &guid, index, &interfaceData); ++index) {
                DWORD required = 0;
                SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, nullptr, 0, &required,
                                                 nullptr);
                if (required == 0) {
                    continue;
                }
                std::vector<BYTE> detailBuffer(required);
                auto* detail =
                    reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
                detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
                SP_DEVINFO_DATA devInfoData{};
                devInfoData.cbSize = sizeof(devInfoData);
                if (!SetupDiGetDeviceInterfaceDetailW(deviceInfo, &interfaceData, detail, required,
                                                      nullptr, &devInfoData)) {
                    continue;
                }
                if (!matchesHardwareId(deviceInfo, &devInfoData, pid)) {
                    continue;
                }

                HANDLE file = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE,
                                          FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                          FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, nullptr);
                if (file == INVALID_HANDLE_VALUE) {
                    continue;
                }
                WINUSB_INTERFACE_HANDLE usbInterface = nullptr;
                if (!WinUsb_Initialize(file, &usbInterface)) {
                    CloseHandle(file);
                    continue;
                }
                USB_INTERFACE_DESCRIPTOR descriptor{};
                if (!WinUsb_QueryInterfaceSettings(usbInterface, 0, &descriptor)) {
                    WinUsb_Free(usbInterface);
                    CloseHandle(file);
                    continue;
                }

                if (descriptor.bInterfaceNumber == 0 && interface0 == nullptr) {
                    file0 = file;
                    interface0 = usbInterface;
                } else if (descriptor.bInterfaceNumber == 1 && interface1 == nullptr) {
                    file1 = file;
                    interface1 = usbInterface;
                } else {
                    WinUsb_Free(usbInterface);
                    CloseHandle(file);
                }

                if (interface0 != nullptr && interface1 != nullptr) {
                    break;
                }
            }
            SetupDiDestroyDeviceInfoList(deviceInfo);
            if (interface0 != nullptr && interface1 != nullptr) {
                break;
            }
        }

        if (interface0 == nullptr) {
            close();
            setError(error, modelName(modelFromPid(pid)) + " camera not found");
            return false;
        }
        DWORD lastAssociatedError = ERROR_SUCCESS;
        if (interface1 == nullptr) {
            // WinUSB's associated-interface index is zero based: index 0 is the
            // interface immediately following the one returned by WinUsb_Initialize.
            for (UCHAR index = 0; index < 2 && interface1 == nullptr; ++index) {
                WINUSB_INTERFACE_HANDLE associated = nullptr;
                if (!WinUsb_GetAssociatedInterface(interface0, index, &associated)) {
                    lastAssociatedError = GetLastError();
                    continue;
                }
                USB_INTERFACE_DESCRIPTOR associatedDescriptor{};
                if (WinUsb_QueryInterfaceSettings(associated, 0, &associatedDescriptor) &&
                    associatedDescriptor.bInterfaceNumber == 1) {
                    interface1 = associated;
                    interface1Associated = true;
                } else {
                    WinUsb_Free(associated);
                }
            }
        }
        if (interface1 == nullptr) {
            close();
            setError(error, "streaming interface not found (associated interface error " +
                                std::to_string(static_cast<unsigned long>(lastAssociatedError)) + ")");
            return false;
        }

        ULONG timeout = kControlTimeoutMs;
        WinUsb_SetPipePolicy(interface0, 0, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        timeout = kBulkTimeoutMs;
        WinUsb_SetPipePolicy(interface1, kEndpointIn, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        return true;
    }

    void close()
    {
        if (interface1 != nullptr) {
            WinUsb_Free(interface1);
            interface1 = nullptr;
        }
        if (interface0 != nullptr) {
            WinUsb_Free(interface0);
            interface0 = nullptr;
        }
        if (file1 != nullptr) {
            CloseHandle(file1);
            file1 = nullptr;
        }
        if (file0 != nullptr) {
            CloseHandle(file0);
            file0 = nullptr;
        }
        interface1Associated = false;
    }

    static Model modelFromPid(std::uint16_t pid)
    {
        return pid == 0x45C2 ? Model::P1 : Model::P3;
    }

    bool controlTransfer(std::uint8_t requestType, std::uint8_t request, std::uint16_t value,
                         std::uint16_t index, std::uint8_t* data, std::uint16_t length,
                         int& transferred, std::string* error)
    {
        WINUSB_SETUP_PACKET setup{};
        setup.RequestType = requestType;
        setup.Request = request;
        setup.Value = value;
        setup.Index = index;
        setup.Length = length;

        ULONG completed = 0;
        if (!WinUsb_ControlTransfer(interface0, setup, data, length, &completed, nullptr)) {
            setError(error, "USB control transfer failed (error " + std::to_string(GetLastError()) + ")");
            return false;
        }
        transferred = static_cast<int>(completed);
        return true;
    }

    bool bulkTransfer(std::uint8_t endpoint, std::uint8_t* data, int length, int timeoutMs,
                      int& transferred, std::string* error)
    {
        ULONG timeout = static_cast<ULONG>(timeoutMs);
        WinUsb_SetPipePolicy(interface1, endpoint, PIPE_TRANSFER_TIMEOUT, sizeof(timeout), &timeout);
        ULONG completed = 0;
        if (!WinUsb_ReadPipe(interface1, endpoint, data, static_cast<ULONG>(length), &completed, nullptr)) {
            setError(error, "USB bulk read failed (error " + std::to_string(GetLastError()) + ")");
            return false;
        }
        transferred = static_cast<int>(completed);
        return true;
    }

    bool setAlternateSetting(int alternateSetting, std::string* error)
    {
        if (!WinUsb_SetCurrentAlternateSetting(interface1, static_cast<UCHAR>(alternateSetting))) {
            setError(error, "Failed to set alternate setting " + std::to_string(alternateSetting));
            return false;
        }
        return true;
    }
};

#else // !_WIN32

struct UsbCameraBackend::Impl {
    libusb_context* context = nullptr;
    libusb_device_handle* handle = nullptr;

    bool open(std::uint16_t pid, std::string* error)
    {
        if (libusb_init(&context) != 0) {
            setError(error, "libusb_init failed");
            return false;
        }
        handle = libusb_open_device_with_vid_pid(context, kUsbVendorId, pid);
        if (handle == nullptr) {
            setError(error, "camera not found");
            libusb_exit(context);
            context = nullptr;
            return false;
        }
        libusb_set_auto_detach_kernel_driver(handle, 1);
        const int configResult = libusb_set_configuration(handle, 1);
        if (configResult != 0 && configResult != LIBUSB_ERROR_BUSY) {
            setError(error, "libusb_set_configuration failed");
            close();
            return false;
        }
        for (int usbInterface : {0, 1}) {
            if (libusb_claim_interface(handle, usbInterface) != 0) {
                setError(error, "libusb_claim_interface failed");
                close();
                return false;
            }
        }
        return true;
    }

    void close()
    {
        if (handle != nullptr) {
            libusb_release_interface(handle, 0);
            libusb_release_interface(handle, 1);
            libusb_close(handle);
            handle = nullptr;
        }
        if (context != nullptr) {
            libusb_exit(context);
            context = nullptr;
        }
    }

    bool controlTransfer(std::uint8_t requestType, std::uint8_t request, std::uint16_t value,
                         std::uint16_t index, std::uint8_t* data, std::uint16_t length,
                         int& transferred, std::string* error)
    {
        const int result = libusb_control_transfer(handle, requestType, request, value, index, data,
                                                   length, kControlTimeoutMs);
        if (result < 0) {
            setError(error, std::string("USB control transfer failed: ") + libusb_error_name(result));
            return false;
        }
        transferred = result;
        return true;
    }

    bool bulkTransfer(std::uint8_t endpoint, std::uint8_t* data, int length, int timeoutMs,
                      int& transferred, std::string* error)
    {
        const int result = libusb_bulk_transfer(handle, endpoint, data, length, &transferred, timeoutMs);
        if (result != 0) {
            setError(error, std::string("USB bulk read failed: ") + libusb_error_name(result));
            return false;
        }
        return true;
    }

    bool setAlternateSetting(int alternateSetting, std::string* error)
    {
        const int result = libusb_set_interface_alt_setting(handle, 1, alternateSetting);
        if (result != 0) {
            setError(error, "Failed to set alternate setting");
            return false;
        }
        return true;
    }
};

#endif

// ---------------------------------------------------------------------------
// UsbCameraBackend
// ---------------------------------------------------------------------------
UsbCameraBackend::UsbCameraBackend(Model model)
    : m_impl(std::make_unique<Impl>())
    , m_config(modelConfig(model))
{
}

UsbCameraBackend::~UsbCameraBackend()
{
    disconnect();
}

bool UsbCameraBackend::connect(std::string* error)
{
    if (m_connected) {
        return true;
    }
    if (!m_impl->open(m_config.pid, error)) {
        return false;
    }

    m_frameBuffer.resize(static_cast<std::size_t>(m_config.frameBufferSize()));
    m_chunkBuffer.resize(static_cast<std::size_t>(kFrameReadChunk));
    m_frameData.resize(static_cast<std::size_t>(m_config.frameReadSize() - kMarkerSize));
    m_controlBuffer.resize(64);

    m_connected = true;
    return true;
}

void UsbCameraBackend::disconnect()
{
    if (m_streaming) {
        stopStreaming();
    }
    m_impl->close();
    m_connected = false;
}

bool UsbCameraBackend::isConnected() const
{
    return m_connected;
}

bool UsbCameraBackend::initialize(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }

    return readRegister(Command::ReadName, 30, m_deviceInfo.model, error) &&
           readRegister(Command::ReadVersion, 12, m_deviceInfo.firmwareVersion, error) &&
           readRegister(Command::ReadPartNumber, 64, m_deviceInfo.partNumber, error) &&
           readRegister(Command::ReadSerial, 64, m_deviceInfo.serial, error) &&
           readRegister(Command::ReadHwVersion, 64, m_deviceInfo.hardwareVersion, error) &&
           readRegister(Command::ReadModelLong, 64, m_deviceInfo.modelLong, error);
}

const DeviceInfo& UsbCameraBackend::deviceInfo() const
{
    return m_deviceInfo;
}

bool UsbCameraBackend::startStreaming(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }

    m_stats = FrameStats();

    std::uint8_t status = 0;
    std::vector<std::uint8_t> response;
    if (!sendCommand(Command::StartStream, error) || !readStatus(status, error) ||
        !readResponse(1, response, error) || !readStatus(status, error)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (!m_impl->setAlternateSetting(1, error)) {
        return false;
    }
    int transferred = 0;
    if (!m_impl->controlTransfer(0x40, 0xEE, 0, 1, nullptr, 0, transferred, error)) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    std::vector<std::uint8_t> discard(static_cast<std::size_t>(m_config.frameSize()));
    m_impl->bulkTransfer(kEndpointIn, discard.data(), static_cast<int>(discard.size()), 100, transferred,
                         error);

    if (!sendCommand(Command::StartStream, error) || !readStatus(status, error) ||
        !readResponse(1, response, error) || !readStatus(status, error)) {
        return false;
    }

    m_streaming = true;
    return true;
}

void UsbCameraBackend::stopStreaming()
{
    if (m_streaming) {
        std::string error;
        m_impl->setAlternateSetting(0, &error);
        m_streaming = false;
    }
}

bool UsbCameraBackend::readFrame(Image16& thermal, Image8& ir, std::string* error)
{
    if (!m_connected || !m_streaming) {
        setError(error, "Camera is not streaming");
        return false;
    }

    const int frameReadSize = m_config.frameReadSize();
    if (!readFrameBuffer(m_frameBuffer, static_cast<std::size_t>(frameReadSize), error)) {
        return false;
    }

    const Marker startMarker = parseMarker(m_frameBuffer.data());
    const Marker endMarker = parseMarker(m_frameBuffer.data() + frameReadSize - kMarkerSize);

    if (startMarker.cnt1 != endMarker.cnt1) {
        ++m_stats.markerMismatches;
        if (m_validateMarkers) {
            setError(error, "Frame marker mismatch");
            return false;
        }
    }

    if (m_stats.framesRead > 0) {
        const std::uint32_t expected = (m_stats.lastCnt3 + kCnt3Increment) % kCnt3Wrap;
        const std::uint32_t difference = (endMarker.cnt3 - expected) % kCnt3Wrap;
        if (difference > static_cast<std::uint32_t>(kCnt3Increment / 2) &&
            difference < static_cast<std::uint32_t>(kCnt3Wrap - kCnt3Increment)) {
            m_stats.framesDropped += difference / kCnt3Increment;
        }
    }

    m_stats.framesRead += 1;
    m_stats.lastCnt1 = startMarker.cnt1;
    m_stats.lastCnt3 = endMarker.cnt3;

    const std::size_t dataSize = static_cast<std::size_t>(frameReadSize - kMarkerSize);
    std::memcpy(m_frameData.data(), m_frameBuffer.data(), dataSize);

    if (!extractBoth(std::span<const std::uint8_t>(m_frameData.data(), m_frameData.size()), m_config, ir,
                     thermal)) {
        setError(error, "Failed to parse frame data");
        return false;
    }
    return true;
}

bool UsbCameraBackend::triggerShutter(std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }

    std::uint8_t status = 0;
    if (!sendCommand(Command::Shutter, error) || !readStatus(status, error)) {
        return false;
    }

    const std::size_t readSize =
        static_cast<std::size_t>(m_config.frameReadSize() + m_config.shutterSegment1());
    if (m_frameBuffer.size() < readSize) {
        m_frameBuffer.resize(readSize);
    }
    return readFrameBuffer(m_frameBuffer, readSize, error);
}

bool UsbCameraBackend::setGainMode(GainMode mode, std::string* error)
{
    if (!m_connected) {
        setError(error, "Camera is not connected");
        return false;
    }

    if (mode == GainMode::Low || mode == GainMode::High) {
        const Command command = mode == GainMode::Low ? Command::GainLow : Command::GainHigh;
        std::uint8_t status = 0;
        if (!sendCommand(command, error) || !readStatus(status, error)) {
            return false;
        }
    }
    m_gainMode = mode;
    return true;
}

GainMode UsbCameraBackend::gainMode() const
{
    return m_gainMode;
}

const ModelConfig& UsbCameraBackend::config() const
{
    return m_config;
}

const FrameStats& UsbCameraBackend::stats() const
{
    return m_stats;
}

bool UsbCameraBackend::sendCommand(Command command, std::string* error)
{
    const auto& bytes = commandBytes(command);
    const std::vector<std::uint8_t> buffer(bytes.begin(), bytes.end());
    int transferred = 0;
    return m_impl->controlTransfer(0x41, 0x20, 0, 0, const_cast<std::uint8_t*>(buffer.data()),
                                   static_cast<std::uint16_t>(buffer.size()), transferred, error);
}

bool UsbCameraBackend::readResponse(std::size_t length, std::vector<std::uint8_t>& out,
                                    std::string* error)
{
    if (m_controlBuffer.size() < length) {
        m_controlBuffer.resize(length);
    }
    int transferred = 0;
    if (!m_impl->controlTransfer(0xC1, 0x21, 0, 0, m_controlBuffer.data(),
                                 static_cast<std::uint16_t>(length), transferred, error)) {
        return false;
    }
    out.assign(m_controlBuffer.begin(), m_controlBuffer.begin() + static_cast<std::ptrdiff_t>(length));
    return true;
}

bool UsbCameraBackend::readStatus(std::uint8_t& status, std::string* error)
{
    int transferred = 0;
    return m_impl->controlTransfer(0xC1, 0x22, 0, 0, &status, 1, transferred, error);
}

bool UsbCameraBackend::readFrameBuffer(std::vector<std::uint8_t>& buffer, std::size_t readSize,
                                       std::string* error)
{
    if (buffer.size() < readSize) {
        buffer.resize(readSize);
    }

    std::size_t position = 0;
    while (position < readSize) {
        int transferred = 0;
        if (!m_impl->bulkTransfer(kEndpointIn, m_chunkBuffer.data(),
                                  static_cast<int>(m_chunkBuffer.size()), kBulkTimeoutMs, transferred,
                                  error)) {
            return false;
        }
        if (transferred <= 0) {
            continue;
        }

        const std::size_t next = position + static_cast<std::size_t>(transferred);
        if ((transferred == kMarkerSize && next < readSize) ||
            (next >= readSize && transferred != kMarkerSize)) {
            position = 0;
            continue;
        }
        std::memcpy(buffer.data() + position, m_chunkBuffer.data(),
                    static_cast<std::size_t>(transferred));
        position = next;
    }
    return true;
}

bool UsbCameraBackend::readRegister(Command command, std::size_t length, QString& out,
                                    std::string* error)
{
    std::uint8_t status = 0;
    std::vector<std::uint8_t> data;
    if (!sendCommand(command, error) || !readStatus(status, error) ||
        !readResponse(length, data, error) || !readStatus(status, error)) {
        return false;
    }

    out = QString::fromLatin1(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()));
    const qsizetype nullIndex = out.indexOf(QChar('\0'));
    if (nullIndex >= 0) {
        out.truncate(nullIndex);
    }
    return true;
}

std::unique_ptr<CameraBackend> makeUsbCameraBackend(Model model)
{
    return std::make_unique<UsbCameraBackend>(model);
}

} // namespace qtthermal
