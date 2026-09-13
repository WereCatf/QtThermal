#include "ui/FixedRangeDialog.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace qtthermal {
namespace {

constexpr double kSensorMinimum = -20.0;
constexpr double kSensorMaximum = 600.0;
constexpr double kDefaultMinimum = 10.0;
constexpr double kDefaultMaximum = 40.0;

} // namespace

FixedRangeDialog::FixedRangeDialog(double minimum, double maximum, bool sceneRangeValid,
                                   double sceneMinimum, double sceneMaximum, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Fixed AGC Range"));

    m_minimum = new QDoubleSpinBox(this);
    m_minimum->setRange(kSensorMinimum, kSensorMaximum);
    m_minimum->setDecimals(1);
    m_minimum->setSingleStep(1.0);
    m_minimum->setSuffix(QStringLiteral(" \u00b0C"));
    m_minimum->setValue(minimum);

    m_maximum = new QDoubleSpinBox(this);
    m_maximum->setRange(kSensorMinimum, kSensorMaximum);
    m_maximum->setDecimals(1);
    m_maximum->setSingleStep(1.0);
    m_maximum->setSuffix(QStringLiteral(" \u00b0C"));
    m_maximum->setValue(maximum);

    auto* sceneLabel = new QLabel(this);
    sceneLabel->setText(sceneRangeValid
                            ? tr("Current scene: %1 \u2013 %2 \u00b0C")
                                  .arg(sceneMinimum, 0, 'f', 1)
                                  .arg(sceneMaximum, 0, 'f', 1)
                            : tr("Current scene: --"));

    auto* resetButton = new QPushButton(tr("Reset to defaults"), this);

    auto* form = new QFormLayout;
    form->addRow(tr("Minimum temperature"), m_minimum);
    form->addRow(tr("Maximum temperature"), m_maximum);
    form->addRow(tr("Range"), sceneLabel);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(resetButton);
    layout->addWidget(buttons);

    connect(resetButton, &QPushButton::clicked, this, &FixedRangeDialog::resetToDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, &FixedRangeDialog::validateAndAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

double FixedRangeDialog::minimum() const
{
    return m_minimum->value();
}

double FixedRangeDialog::maximum() const
{
    return m_maximum->value();
}

void FixedRangeDialog::resetToDefaults()
{
    m_minimum->setValue(kDefaultMinimum);
    m_maximum->setValue(kDefaultMaximum);
}

void FixedRangeDialog::validateAndAccept()
{
    if (m_maximum->value() <= m_minimum->value()) {
        QMessageBox::warning(this, tr("Fixed AGC Range"),
                             tr("The maximum temperature must be greater than the minimum."));
        return;
    }
    accept();
}

} // namespace qtthermal
