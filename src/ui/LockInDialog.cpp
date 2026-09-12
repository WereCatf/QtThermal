#include "ui/LockInDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLineEdit>

namespace qtthermal {

LockInDialog::LockInDialog(const LockInConfig& config, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Lock-In Configuration"));

    m_port = new QLineEdit(QString::fromStdString(config.port), this);

    m_baudRate = new QComboBox(this);
    for (int baud : {9600, 19200, 38400, 57600, 115200, 230400}) {
        m_baudRate->addItem(QString::number(baud), baud);
    }
    const int baudIndex = m_baudRate->findData(config.baudRate);
    m_baudRate->setCurrentIndex(baudIndex >= 0 ? baudIndex : m_baudRate->count() - 1);

    m_period = new QDoubleSpinBox(this);
    m_period->setRange(0.01, 60.0);
    m_period->setDecimals(3);
    m_period->setSuffix(QStringLiteral(" s"));
    m_period->setValue(config.period);

    m_integration = new QDoubleSpinBox(this);
    m_integration->setRange(1.0, 3600.0);
    m_integration->setDecimals(1);
    m_integration->setSuffix(QStringLiteral(" s"));
    m_integration->setValue(config.integration);

    m_invert = new QCheckBox(tr("Invert control output"), this);
    m_invert->setChecked(config.invert);

    auto* layout = new QFormLayout(this);
    layout->addRow(tr("Serial port"), m_port);
    layout->addRow(tr("Baud rate"), m_baudRate);
    layout->addRow(tr("Period"), m_period);
    layout->addRow(tr("Integration"), m_integration);
    layout->addRow(QString(), m_invert);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addRow(buttons);
}

LockInConfig LockInDialog::config() const
{
    LockInConfig result;
    result.port = m_port->text().toStdString();
    result.baudRate = m_baudRate->currentData().toInt();
    result.period = m_period->value();
    result.integration = m_integration->value();
    result.invert = m_invert->isChecked();
    return result;
}

} // namespace qtthermal
