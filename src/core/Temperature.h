#pragma once

#include "core/CameraTypes.h"

namespace qtthermal {

/// Convert a raw sensor value (1/64 K) to Kelvin.
[[nodiscard]] double rawToKelvin(double raw);

/// Convert Kelvin to Celsius.
[[nodiscard]] double kelvinToCelsius(double kelvin);

/// Convert Celsius to Kelvin.
[[nodiscard]] double celsiusToKelvin(double celsius);

/// Convert a raw sensor value directly to Celsius.
[[nodiscard]] double rawToCelsius(double raw);

/// Convert Celsius to a raw sensor value.
[[nodiscard]] std::int32_t celsiusToRaw(double celsius);

/// Apply the Stefan-Boltzmann radiometric correction to an apparent temperature.
[[nodiscard]] double applyEmissivityCorrection(double apparentTempK, double emissivity,
                                               double reflectedTempC = 25.0);

/// Convert a raw sensor value to Celsius applying the supplied environment.
[[nodiscard]] double rawToCelsiusCorrected(double raw, const EnvParams& env);

} // namespace qtthermal
