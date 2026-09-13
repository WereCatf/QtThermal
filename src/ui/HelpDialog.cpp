#include "ui/HelpDialog.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace qtthermal {

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("QtThermal Help"));
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    resize(780, 640);

    m_browser = new QTextBrowser(this);
    m_browser->setOpenExternalLinks(true);
    m_browser->setSource(QUrl(QStringLiteral("qrc:/help/help.html")));

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_browser);
    layout->addWidget(buttons);
}

} // namespace qtthermal
