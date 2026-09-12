#pragma once

#include <QImage>
#include <QWidget>

namespace qtthermal {

/// Widget that displays a thermal frame scaled to fit while preserving aspect.
class ThermalView : public QWidget {
    Q_OBJECT

public:
    explicit ThermalView(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void clear();
    [[nodiscard]] QImage currentImage() const;

    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QImage m_image;
};

} // namespace qtthermal
