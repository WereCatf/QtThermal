#pragma once

#include <QImage>
#include <QString>
#include <QWidget>

namespace qtthermal {

/// Widget that displays a thermal frame scaled to fit while preserving aspect.
class ThermalView : public QWidget {
    Q_OBJECT

public:
    explicit ThermalView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void clear();
    void setPlaceholderText(const QString& text);
    [[nodiscard]] QImage currentImage() const;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
    QString m_placeholder = tr("No signal");
};

} // namespace qtthermal
