#pragma once

#include <QDialog>

class QSpinBox;

namespace qtthermal {

/// Dialog for choosing how many hot and cold spots to track.
class HotspotDialog : public QDialog {
    Q_OBJECT

public:
    HotspotDialog(int hotCount, int coldCount, QWidget* parent = nullptr);

    [[nodiscard]] int hotCount() const;
    [[nodiscard]] int coldCount() const;

private:
    QSpinBox* m_hotCount = nullptr;
    QSpinBox* m_coldCount = nullptr;
};

} // namespace qtthermal
