#include "ui/ThermalView.h"

#include <QPainter>
#include <QPaintEvent>

namespace qtthermal {

ThermalView::ThermalView(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setMinimumSize(320, 240);
}

void ThermalView::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void ThermalView::clear()
{
    m_image = QImage();
    update();
}

QImage ThermalView::currentImage() const
{
    return m_image;
}

QSize ThermalView::minimumSizeHint() const
{
    return {320, 240};
}

void ThermalView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (m_image.isNull()) {
        painter.setPen(Qt::gray);
        painter.drawText(rect(), Qt::AlignCenter, tr("No signal"));
        return;
    }

    const QSize target = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    const QRect targetRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2),
                           target);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(targetRect, m_image);
}

} // namespace qtthermal
