#pragma once

#include <QDialog>

class QDoubleSpinBox;

namespace qtthermal {

/// Dialog for editing emissivity and reflected temperature.
class EmissivityDialog : public QDialog {
    Q_OBJECT

public:
    EmissivityDialog(double emissivity, double reflectedTemp, QWidget* parent = nullptr);

    [[nodiscard]] double emissivity() const;
    [[nodiscard]] double reflectedTemp() const;

private:
    QDoubleSpinBox* m_emissivity = nullptr;
    QDoubleSpinBox* m_reflectedTemp = nullptr;
};

} // namespace qtthermal
