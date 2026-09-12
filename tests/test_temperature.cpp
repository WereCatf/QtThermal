#include "core/Temperature.h"

#include <QtTest>

using namespace qtthermal;

class TemperatureTest : public QObject {
    Q_OBJECT

private slots:
    void rawToKelvinZero();
    void rawToKelvinRoomTemp();
    void kelvinToCelsiusConversion();
    void rawToCelsiusFreezing();
    void celsiusToRawRoundTrip();
    void emissivityBlackbodyUnchanged();
    void emissivityLowerIncreasesTemperature();
    void emissivityAtReflectedHasNoEffect();
    void correctedConversion();
};

void TemperatureTest::rawToKelvinZero()
{
    QCOMPARE(rawToKelvin(0.0), 0.0);
}

void TemperatureTest::rawToKelvinRoomTemp()
{
    const double raw = 298.15 * kTempScale;
    QVERIFY(qAbs(rawToKelvin(raw) - 298.15) < 1e-3);
}

void TemperatureTest::kelvinToCelsiusConversion()
{
    QVERIFY(qAbs(kelvinToCelsius(273.15)) < 1e-9);
    QVERIFY(qAbs(kelvinToCelsius(373.15) - 100.0) < 1e-9);
}

void TemperatureTest::rawToCelsiusFreezing()
{
    const double raw = (0.0 + kKelvinOffset) * kTempScale;
    QVERIFY(qAbs(rawToCelsius(raw)) < 0.02);
}

void TemperatureTest::celsiusToRawRoundTrip()
{
    for (const double temperature : {-40.0, 0.0, 25.0, 37.0, 100.0, 200.0}) {
        const double recovered = rawToCelsius(static_cast<double>(celsiusToRaw(temperature)));
        QVERIFY(qAbs(recovered - temperature) < 0.02);
    }
}

void TemperatureTest::emissivityBlackbodyUnchanged()
{
    const double temperature = 300.0;
    QCOMPARE(applyEmissivityCorrection(temperature, 1.0), temperature);
}

void TemperatureTest::emissivityLowerIncreasesTemperature()
{
    const double apparent = 323.15;
    const double result95 = applyEmissivityCorrection(apparent, 0.95, 25.0);
    const double result80 = applyEmissivityCorrection(apparent, 0.80, 25.0);
    QVERIFY(result80 > result95);
    QVERIFY(result95 > apparent);
}

void TemperatureTest::emissivityAtReflectedHasNoEffect()
{
    const double reflected = 25.0;
    const double apparent = reflected + kKelvinOffset;
    const double result = applyEmissivityCorrection(apparent, 0.95, reflected);
    QVERIFY(qAbs(result - apparent) < 0.01);
}

void TemperatureTest::correctedConversion()
{
    const double raw = (50.0 + kKelvinOffset) * kTempScale;
    EnvParams blackbody;
    blackbody.emissivity = 1.0;
    EnvParams greybody;
    greybody.emissivity = 0.95;
    QVERIFY(rawToCelsiusCorrected(raw, greybody) > rawToCelsiusCorrected(raw, blackbody));
}

QTEST_APPLESS_MAIN(TemperatureTest)

#include "test_temperature.moc"
