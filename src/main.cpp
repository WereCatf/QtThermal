#include "core/CameraTypes.h"
#include "ui/MainWindow.h"
#include "version.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setOrganizationName(QStringLiteral(QTTHERMAL_ORGANIZATION_NAME));
    QApplication::setOrganizationDomain(QStringLiteral(QTTHERMAL_ORGANIZATION_DOMAIN));
    QApplication::setApplicationName(QStringLiteral(QTTHERMAL_APPLICATION_NAME));
    QApplication::setApplicationVersion(QStringLiteral(QTTHERMAL_VERSION_STRING));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Qt6 thermal camera viewer for P3-series USB cameras"));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption modelOption({QStringLiteral("m"), QStringLiteral("model")},
                                         QStringLiteral("Camera model (p1 or p3)."),
                                         QStringLiteral("model"), QStringLiteral("p3"));
    const QCommandLineOption simulateOption({QStringLiteral("s"), QStringLiteral("simulate")},
                                            QStringLiteral("Run with the simulated backend (no hardware)."));
    parser.addOption(modelOption);
    parser.addOption(simulateOption);
    parser.process(application);

    qtthermal::Model model = qtthermal::Model::P3;
    const bool modelSpecified = parser.isSet(modelOption);
    if (modelSpecified && !qtthermal::parseModel(parser.value(modelOption).toStdString(), model)) {
        qWarning() << "Unknown camera model:" << parser.value(modelOption);
        return 1;
    }

    const bool simulate = parser.isSet(simulateOption);

    qtthermal::MainWindow window(model, simulate, modelSpecified);
    window.show();
    return QApplication::exec();
}
