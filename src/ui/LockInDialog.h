#pragma once

#include "lockin/LockInController.h"

#include <QDialog>

#include <string>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;

namespace qtthermal {

/// Dialog for editing the lock-in serial configuration.
class LockInDialog : public QDialog {
    Q_OBJECT

public:
    explicit LockInDialog(const LockInConfig& config, QWidget* parent = nullptr);

    [[nodiscard]] LockInConfig config() const;

private:
    QLineEdit* m_port = nullptr;
    QComboBox* m_baudRate = nullptr;
    QDoubleSpinBox* m_period = nullptr;
    QDoubleSpinBox* m_integration = nullptr;
    QCheckBox* m_invert = nullptr;
};

} // namespace qtthermal
