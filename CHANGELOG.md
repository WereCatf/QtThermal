# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-09-12

### Added

- Native Qt6 Widgets application for P3-series USB thermal cameras.
- USB protocol implementation with libusb, including frame marker validation.
- Support for P1 (160x120) and P3 (256x192) camera models.
- Temperature conversion with emissivity correction.
- Image pipeline with selectable colormaps, AGC modes, detail enhancement,
  temporal noise reduction and local contrast enhancement.
- Rotate, mirror, zoom and interpolation controls exposed through the menu bar.
- Shutter/NUC and high/low gain camera control.
- Optional lock-in thermography with a serial load controller.
- Persistent settings, including window size and position.
- Simulated camera backend for hardware-free runs.

[Unreleased]: https://example.invalid/QtThermal/compare/v0.1.0...HEAD
[0.1.0]: https://example.invalid/QtThermal/releases/tag/v0.1.0
