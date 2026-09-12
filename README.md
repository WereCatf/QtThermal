# QtThermal

A native Qt6 desktop application for P3-series USB thermal cameras, with
real-time display, radiometric temperature readout and optional lock-in
thermography.

## Features

- USB driver for frame capture and device control
- Real-time thermal viewer with six colormaps
- Temperature measurement at the image centre with emissivity correction
- Temporal noise reduction and digital detail enhancement
- Local contrast enhancement (CLAHE) and selectable interpolation
- Multiple AGC modes (hardware, temporal percentile, fixed range)
- Rotation, mirroring and zoom through the menu bar
- Shutter/NUC calibration and high/low gain control
- Optional lock-in thermography for resolving very small temperature changes
- Screenshot and 16-bit raw data export
- All options and the window geometry are remembered between sessions

## Devices

| Model | VID    | PID    | Resolution |
|-------|--------|--------|------------|
| P1    | 0x3474 | 0x45C2 | 160 x 120  |
| P3    | 0x3474 | 0x45A2 | 256 x 192  |

## Building

Requirements:

- Qt 6.5 or newer (Core, Gui, Widgets and, for tests, Test)
- CMake 3.21 or newer and Ninja
- A C++20 compiler
- On Windows: the WinUSB API (part of the Windows SDK)
- On other platforms: libusb-1.0 development files

On Windows with the official Qt MinGW kit:

```bash
cmake --preset windows-mingw
cmake --build --preset windows-mingw
ctest --preset windows-mingw
```

For other Qt installations, set `QTDIR` to the Qt prefix and use the
`default` preset:

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

## Running

```bash
# P3 camera (default)
QtThermal

# P1 camera
QtThermal --model p1

# Run without hardware using the simulated backend
QtThermal --simulate
```

### Command line options

| Option        | Description                                  |
|---------------|----------------------------------------------|
| `--model`     | Camera model, `p1` or `p3` (default: `p3`)   |
| `--simulate`  | Use the simulated backend, no hardware       |
| `--help`      | Show the help message                        |
| `--version`   | Show the version                             |

## Using the application

The menu bar exposes every function; no keyboard shortcuts are required.

- **File** - save a screenshot (PNG) or dump the current 16-bit thermal frame
  (PGM).
- **View** - colormap, rotation, mirroring, zoom, interpolation, reticule,
  colorbar and min/max hotspot markers.
- **Processing** - AGC mode and fixed range, enhanced mode (CLAHE + DDE),
  detail enhancement, temporal noise reduction and emissivity.
- **Camera** - model selection, reconnect, shutter/NUC, gain mode and device
  information.
- **Lock-In** - start/stop lock-in acquisition and edit its configuration.
- **Help** - application and Qt information.

All settings, including the window size and position, are stored using
`QSettings` and restored on the next start.

## USB permissions

On Linux, add a udev rule allowing non-root access:

```bash
sudo tee /etc/udev/rules.d/99-p3-ir.rules << EOF
# P1 camera
SUBSYSTEM=="usb", ATTR{idVendor}=="3474", ATTR{idProduct}=="45c2", MODE="0666"
# P3 camera
SUBSYSTEM=="usb", ATTR{idVendor}=="3474", ATTR{idProduct}=="45a2", MODE="0666"
EOF
sudo udevadm control --reload-rules
sudo udevadm trigger
```

On Windows, install the WinUSB driver with [Zadig](https://zadig.akeo.ie/):
select `Options -> List All Devices`, choose the camera (VID 3474, PID 45A2 or
45C2) and replace its driver with WinUSB.

## Architecture

```
src/core    protocol, frame parsing, temperature, colormaps, image pipeline
src/camera  camera backends (WinUSB/libusb and simulated) and capture worker
src/lockin  lock-in controller and serial transport
src/ui      main window, thermal view and dialogs
tests       QtTest unit tests
```

`CameraBackend` abstracts the hardware. `CaptureWorker` runs the blocking USB
reads and image pipeline on a worker thread and sends finished `QImage` frames
to the window. The lock-in controller runs its own thread and consumes frames
forwarded by the capture worker.

## Documentation

- [docs/P3_PROTOCOL.md](docs/P3_PROTOCOL.md) - USB protocol reference
- [docs/LOCK-IN.md](docs/LOCK-IN.md) - lock-in thermography usage

## License

Apache-2.0. See [LICENSE](LICENSE).
