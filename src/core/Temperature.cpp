#include "core/Temperature.h"

#include <algorithm>
#include <cmath>

namespace qtthermal {

double rawToKelvin(double raw)
{
    return raw / kTempScale;
}

double kelvinToCelsius(double kelvin)
{
    return kelvin - kKelvinOffset;
}

double celsiusToKelvin(double celsius)
{
    return celsius + kKelvinOffset;
}

double rawToCelsius(double raw)
{
    return kelvinToCelsius(rawToKelvin(raw));
}

std::int32_t celsiusToRaw(double celsius)
{
    return static_cast<std::int32_t>((celsius + kKelvinOffset) * kTempScale);
}

double applyEmissivityCorrection(double apparentTempK, double emissivity, double reflectedTempC)
{
    if (emissivity >= 1.0 || emissivity <= 0.0) {
        return apparentTempK;
    }

    const double reflectedTempK = celsiusToKelvin(reflectedTempC);
    const double apparent4 = std::pow(apparentTempK, 4.0);
    const double reflected4 = std::pow(reflectedTempK, 4.0);
    double object4 = (apparent4 - (1.0 - emissivity) * reflected4) / emissivity;
    object4 = std::max(object4, 0.0);
    return std::pow(object4, 0.25);
}

double rawToCelsiusCorrected(double raw, const EnvParams& env)
{
    const double apparentKelvin = rawToKelvin(raw);
    const double correctedKelvin =
        applyEmissivityCorrection(apparentKelvin, env.emissivity, env.reflectedTemp);
    return kelvinToCelsius(correctedKelvin);
}

} // namespace qtthermal
