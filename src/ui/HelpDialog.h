#pragma once

#include <QDialog>

class QTextBrowser;

namespace qtthermal {

/// Modeless help window showing the embedded user guide.
class HelpDialog : public QDialog {
    Q_OBJECT

public:
    explicit HelpDialog(QWidget* parent = nullptr);

private:
    QTextBrowser* m_browser = nullptr;
};

} // namespace qtthermal
