# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.2] - 2026-09-12

### Fixed

- Show the real frame rate in the image overlay instead of 0.0 FPS.
- Use the correct defaults (Ironbow colormap, bicubic interpolation, high gain)
  when no settings have been saved yet.
- Render an actual colourbar gradient and keep its tick labels inside the image.
- Keep the colourbar geometry stable when changing interpolation or rotation.

### Removed

- Frame and dropped-frame counter from the status bar; the frame rate is
  sufficient.

## [0.1.1] - 2026-09-12

### Fixed

- Retrieve the WinUSB streaming interface through the correct associated
  interface index, so the application connects to P3-series cameras on Windows.

### Changed

- Log camera connection state and failures for easier diagnostics.

## [0.1.0] - 2026-09-12

### Added

- Native Qt6 Widgets application for P3-series USB thermal cameras.
- USB protocol implementation with a native transport (WinUSB on Windows,
  libusb elsewhere), including frame marker validation.
- Support for P1 (160x120) and P3 (256x192) camera models.
- Temperature conversion with emissivity correction.
- Image pipeline with selectable colormaps, AGC modes, detail enhancement,
  temporal noise reduction and local contrast enhancement.
- Rotate, mirror, zoom and interpolation controls exposed through the menu bar.
- Shutter/NUC and high/low gain camera control.
- Optional lock-in thermography with a serial load controller.
- Persistent settings, including window size and position.
- Simulated camera backend for hardware-free runs.

[Unreleased]: https://example.invalid/QtThermal/compare/v0.1.2...HEAD
[0.1.2]: https://example.invalid/QtThermal/releases/tag/v0.1.2
[0.1.1]: https://example.invalid/QtThermal/releases/tag/v0.1.1
[0.1.0]: https://example.invalid/QtThermal/releases/tag/v0.1.0
