#include "ui/HotspotDialog.h"

#include "core/ImageProcessor.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>

namespace qtthermal {

HotspotDialog::HotspotDialog(int hotCount, int coldCount, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Tracked Hotspots"));

    m_hotCount = new QSpinBox(this);
    m_hotCount->setRange(1, kMaxTrackedHotspots);
    m_hotCount->setValue(hotCount);

    m_coldCount = new QSpinBox(this);
    m_coldCount->setRange(1, kMaxTrackedHotspots);
    m_coldCount->setValue(coldCount);

    auto* form = new QFormLayout;
    form->addRow(tr("Hot spots"), m_hotCount);
    form->addRow(tr("Cold spots"), m_coldCount);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(buttons);
}

int HotspotDialog::hotCount() const
{
    return m_hotCount->value();
}

int HotspotDialog::coldCount() const
{
    return m_coldCount->value();
}

} // namespace qtthermal
