#include "ui/EmissivityDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>

namespace qtthermal {

EmissivityDialog::EmissivityDialog(double emissivity, double reflectedTemp, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Emissivity"));

    m_emissivity = new QDoubleSpinBox(this);
    m_emissivity->setRange(0.01, 1.0);
    m_emissivity->setSingleStep(0.01);
    m_emissivity->setDecimals(2);
    m_emissivity->setValue(emissivity);

    m_reflectedTemp = new QDoubleSpinBox(this);
    m_reflectedTemp->setRange(-40.0, 200.0);
    m_reflectedTemp->setDecimals(1);
    m_reflectedTemp->setSuffix(QStringLiteral(" \u00b0C"));
    m_reflectedTemp->setValue(reflectedTemp);

    auto* layout = new QFormLayout(this);
    layout->addRow(tr("Emissivity"), m_emissivity);
    layout->addRow(tr("Reflected temperature"), m_reflectedTemp);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

double EmissivityDialog::emissivity() const
{
    return m_emissivity->value();
}

double EmissivityDialog::reflectedTemp() const
{
    return m_reflectedTemp->value();
}

} // namespace qtthermal
