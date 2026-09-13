#pragma once

#include <QDialog>

class QDoubleSpinBox;
class QLabel;

namespace qtthermal {

/// Dialog for editing the fixed AGC temperature window.
///
/// The spin boxes are limited to the P3 sensor range (-20 to 600 degrees
/// Celsius). A hint shows the current scene temperature range to help pick a
/// window that contains it.
class FixedRangeDialog : public QDialog {
    Q_OBJECT

public:
    FixedRangeDialog(double minimum, double maximum, bool sceneRangeValid, double sceneMinimum,
                     double sceneMaximum, QWidget* parent = nullptr);

    [[nodiscard]] double minimum() const;
    [[nodiscard]] double maximum() const;

private:
    void resetToDefaults();
    void validateAndAccept();

    QDoubleSpinBox* m_minimum = nullptr;
    QDoubleSpinBox* m_maximum = nullptr;
};

} // namespace qtthermal
