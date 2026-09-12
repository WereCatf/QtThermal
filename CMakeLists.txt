cmake_minimum_required(VERSION 3.21)

project(
    QtThermal
    VERSION 0.1.0
    DESCRIPTION "Qt6 thermal camera viewer for P3-series USB thermal cameras"
    HOMEPAGE_URL "https://example.invalid/QtThermal"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

include(GNUInstallDirs)

option(QTTHERMAL_BUILD_TESTS "Build the QtThermal unit tests" ON)
option(QTTHERMAL_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------------------
# USB transport
# ---------------------------------------------------------------------------
# Windows talks to the WinUSB driver directly. Other platforms use libusb-1.0.
if(WIN32)
    set(QTTHERMAL_USB_LIBRARIES winusb setupapi)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)
        set(QTTHERMAL_USB_LIBRARIES PkgConfig::LIBUSB)
    else()
        find_package(libusb-1.0 REQUIRED)
        set(QTTHERMAL_USB_LIBRARIES libusb-1.0::libusb-1.0)
    endif()
endif()

# ---------------------------------------------------------------------------
# Version header
# ---------------------------------------------------------------------------
configure_file(cmake/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/version.h" @ONLY)

# ---------------------------------------------------------------------------
# Warnings
# ---------------------------------------------------------------------------
function(qtthermal_enable_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(QTTHERMAL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(QTTHERMAL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

# ---------------------------------------------------------------------------
# Core library (driver, processing, lock-in)
# ---------------------------------------------------------------------------
qt_add_library(qtthermal_lib STATIC
    src/core/CameraTypes.h
    src/core/CameraTypes.cpp
    src/core/Temperature.h
    src/core/Temperature.cpp
    src/core/Protocol.h
    src/core/Protocol.cpp
    src/core/FrameParser.h
    src/core/FrameParser.cpp
    src/core/Colormap.h
    src/core/Colormap.cpp
    src/core/ImageProcessor.h
    src/core/ImageProcessor.cpp
    src/core/SerialPort.h
    src/core/SerialPort.cpp
    src/camera/CameraBackend.h
    src/camera/UsbCameraBackend.h
    src/camera/UsbCameraBackend.cpp
    src/camera/SimulatedCameraBackend.h
    src/camera/SimulatedCameraBackend.cpp
    src/camera/CaptureWorker.h
    src/camera/CaptureWorker.cpp
    src/lockin/LockInController.h
    src/lockin/LockInController.cpp
)

target_include_directories(qtthermal_lib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(qtthermal_lib
    PUBLIC
        Qt6::Core
        Qt6::Gui
    PRIVATE
        ${QTTHERMAL_USB_LIBRARIES}
)

qtthermal_enable_warnings(qtthermal_lib)

# ---------------------------------------------------------------------------
# Main application
# ---------------------------------------------------------------------------
qt_add_executable(QtThermal
    src/main.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ThermalView.h
    src/ui/ThermalView.cpp
    src/ui/EmissivityDialog.h
    src/ui/EmissivityDialog.cpp
    src/ui/LockInDialog.h
    src/ui/LockInDialog.cpp
)

target_link_libraries(QtThermal PRIVATE
    qtthermal_lib
    Qt6::Widgets
)

set_target_properties(QtThermal PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    OUTPUT_NAME "QtThermal"
)

qtthermal_enable_warnings(QtThermal)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(QTTHERMAL_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Install / deployment
# ---------------------------------------------------------------------------
install(TARGETS QtThermal
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION VERSION_GREATER_EQUAL 6.3)
    qt_generate_deploy_app_script(
        TARGET QtThermal
        OUTPUT_SCRIPT deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${deploy_script})
endif()
