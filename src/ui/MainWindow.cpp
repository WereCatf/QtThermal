#include "ui/MainWindow.h"

#include "core/Temperature.h"
#include "ui/EmissivityDialog.h"
#include "ui/FixedRangeDialog.h"
#include "ui/HelpDialog.h"
#include "ui/LockInDialog.h"
#include "ui/ThermalView.h"
#include "version.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSettings>
#include <QStatusBar>
#include <QThread>

#include <functional>
#include <string>

namespace qtthermal {
namespace {

} // namespace

MainWindow::MainWindow(Model model, bool simulate, bool modelSpecified, QWidget* parent)
    : QMainWindow(parent)
    , m_model(model)
    , m_simulate(simulate)
    , m_modelSpecified(modelSpecified)
{
    m_lockInDefaults.port = defaultLockInPort();
    m_lockInConfig = m_lockInDefaults;

    m_view = new ThermalView(this);
    setCentralWidget(m_view);

    resize(1000, 720);
    loadSettings();
    buildMenus();
    buildStatusBar();
    updateWindowTitle();
    startCapture();
}

MainWindow::~MainWindow()
{
    stopCapture();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    stopLockIn();
    stopCapture();
    saveSettings();
    event->accept();
}

void MainWindow::onFrameReady(const QImage& image)
{
    m_lastImage = image;
    m_view->setImage(image);
    if (m_connectionState != ConnectionState::Streaming) {
        setConnectionState(ConnectionState::Streaming);
    }
}

void MainWindow::onStatusUpdated(double fps, double spotTemp, double minTemp, double maxTemp)
{
    m_fpsLabel->setText(tr("%1 FPS").arg(fps, 0, 'f', 1));
    m_spotLabel->setText(tr("%1 \u00b0C").arg(spotTemp, 0, 'f', 1));
    m_sceneMinTemp = minTemp;
    m_sceneMaxTemp = maxTemp;
    m_sceneRangeValid = true;
}

void MainWindow::onConnectionChanged(bool connected)
{
    if (!connected) {
        setConnectionState(ConnectionState::Disconnected);
    }
    // While connected, keep the "Connecting..." state until the first frame
    // arrives and proves that thermal data is actually flowing.
}

void MainWindow::setConnectionState(ConnectionState state)
{
    m_connectionState = state;
    switch (state) {
    case ConnectionState::Disconnected:
        m_connectionLabel->setText(tr("Disconnected"));
        m_view->setPlaceholderText(tr("No signal"));
        break;
    case ConnectionState::Connecting:
        m_connectionLabel->setText(tr("Connecting..."));
        m_view->setPlaceholderText(tr("Connecting..."));
        break;
    case ConnectionState::Streaming:
        m_connectionLabel->setText(tr("Connected"));
        break;
    }
}

void MainWindow::onDeviceInfoReady(const QString& model, const QString& firmware,
                                   const QString& partNumber, const QString& serial,
                                   const QString& hardware, const QString& modelLong)
{
    m_deviceModel = model;
    m_deviceFirmware = firmware;
    m_devicePartNumber = partNumber;
    m_deviceSerial = serial;
    m_deviceHardware = hardware;
    m_deviceModelLong = modelLong;
}

void MainWindow::onErrorOccurred(const QString& message)
{
    statusBar()->showMessage(message, 5000);
}

void MainWindow::onRawFrameReady(const QVector<quint16>& data, int width, int height)
{
    if (m_pendingDumpPath.isEmpty()) {
        return;
    }

    QFile file(m_pendingDumpPath);
    if (!file.open(QIODevice::WriteOnly)) {
        statusBar()->showMessage(tr("Failed to write %1").arg(m_pendingDumpPath), 5000);
        m_pendingDumpPath.clear();
        return;
    }

    file.write(QStringLiteral("P5\n%1 %2\n65535\n").arg(width).arg(height).toLatin1());
    QByteArray buffer(static_cast<int>(data.size() * 2), Qt::Uninitialized);
    for (int i = 0; i < data.size(); ++i) {
        const quint16 value = data.at(i);
        buffer[2 * i] = static_cast<char>((value >> 8) & 0xFF);
        buffer[2 * i + 1] = static_cast<char>(value & 0xFF);
    }
    file.write(buffer);
    file.close();

    statusBar()->showMessage(tr("Saved %1").arg(m_pendingDumpPath), 5000);
    m_pendingDumpPath.clear();
}

void MainWindow::onLockInFinished()
{
    if (m_worker != nullptr) {
        m_worker->setLockInActive(false);
    }
    if (m_lockInStartAction != nullptr) {
        m_lockInStartAction->setEnabled(true);
    }
    if (m_lockInStopAction != nullptr) {
        m_lockInStopAction->setEnabled(false);
    }
    statusBar()->showMessage(tr("Lock-in finished"), 5000);
}

void MainWindow::buildMenus()
{
    auto addChoice = [this](QMenu* menu, QActionGroup* group, const QString& text, bool checked,
                            std::function<void()> handler) {
        QAction* action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [handler]() { handler(); });
        return action;
    };

    auto addToggle = [this](QMenu* menu, const QString& text, bool checked,
                            std::function<void(bool)> handler) {
        QAction* action = menu->addAction(text);
        action->setCheckable(true);
        action->setChecked(checked);
        connect(action, &QAction::triggered, this, [handler](bool on) { handler(on); });
        return action;
    };

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* screenshotAction = fileMenu->addAction(tr("Save &Screenshot..."));
    connect(screenshotAction, &QAction::triggered, this, &MainWindow::saveScreenshot);
    QAction* dumpAction = fileMenu->addAction(tr("Dump &Raw Data..."));
    connect(dumpAction, &QAction::triggered, this, &MainWindow::requestRawDump);
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(tr("&Quit"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));

    QMenu* colormapMenu = viewMenu->addMenu(tr("&Colormap"));
    auto* colormapGroup = new QActionGroup(this);
    for (const Colormap colormap : {Colormap::WhiteHot, Colormap::BlackHot, Colormap::Rainbow,
                                    Colormap::Ironbow, Colormap::Military, Colormap::Sepia}) {
        addChoice(colormapMenu, colormapGroup, colormapName(colormap), m_params.colormap == colormap,
                  [this, colormap]() {
                      m_params.colormap = colormap;
                      pushParams();
                  });
    }

    QMenu* rotationMenu = viewMenu->addMenu(tr("&Rotation"));
    auto* rotationGroup = new QActionGroup(this);
    for (const int rotation : {0, 90, 180, 270}) {
        addChoice(rotationMenu, rotationGroup, tr("%1\u00b0").arg(rotation), m_params.rotation == rotation,
                  [this, rotation]() {
                      m_params.rotation = rotation;
                      pushParams();
                  });
    }

    addToggle(viewMenu, tr("&Mirror"), m_params.mirror, [this](bool on) {
        m_params.mirror = on;
        pushParams();
    });

    QMenu* zoomMenu = viewMenu->addMenu(tr("&Zoom"));
    auto* zoomGroup = new QActionGroup(this);
    for (int zoom = 1; zoom <= 5; ++zoom) {
        addChoice(zoomMenu, zoomGroup, tr("%1x").arg(zoom), m_params.zoom == zoom, [this, zoom]() {
            m_params.zoom = zoom;
            pushParams();
        });
    }

    QMenu* scaleMenu = viewMenu->addMenu(tr("&Interpolation"));
    auto* scaleGroup = new QActionGroup(this);
    addChoice(scaleMenu, scaleGroup, tr("Off"), m_params.scaleMode == ScaleMode::Off, [this]() {
        m_params.scaleMode = ScaleMode::Off;
        pushParams();
    });
    addChoice(scaleMenu, scaleGroup, tr("Nearest"), m_params.scaleMode == ScaleMode::Nearest, [this]() {
        m_params.scaleMode = ScaleMode::Nearest;
        pushParams();
    });
    addChoice(scaleMenu, scaleGroup, tr("Bilinear"), m_params.scaleMode == ScaleMode::Bilinear, [this]() {
        m_params.scaleMode = ScaleMode::Bilinear;
        pushParams();
    });
    addChoice(scaleMenu, scaleGroup, tr("Bicubic"), m_params.scaleMode == ScaleMode::Bicubic, [this]() {
        m_params.scaleMode = ScaleMode::Bicubic;
        pushParams();
    });
    addChoice(scaleMenu, scaleGroup, tr("Lanczos"), m_params.scaleMode == ScaleMode::Lanczos, [this]() {
        m_params.scaleMode = ScaleMode::Lanczos;
        pushParams();
    });

    addToggle(viewMenu, tr("&Reticule"), m_params.showReticule, [this](bool on) {
        m_params.showReticule = on;
        pushParams();
    });
    addToggle(viewMenu, tr("&Colorbar"), m_params.showColorbar, [this](bool on) {
        m_params.showColorbar = on;
        pushParams();
    });

    QMenu* hotspotMenu = viewMenu->addMenu(tr("&Hotspots"));
    auto* hotspotGroup = new QActionGroup(this);
    addChoice(hotspotMenu, hotspotGroup, tr("Off"), m_params.hotspot == HotspotMode::Off, [this]() {
        m_params.hotspot = HotspotMode::Off;
        pushParams();
    });
    addChoice(hotspotMenu, hotspotGroup, tr("Maximum"), m_params.hotspot == HotspotMode::Max, [this]() {
        m_params.hotspot = HotspotMode::Max;
        pushParams();
    });
    addChoice(hotspotMenu, hotspotGroup, tr("Minimum"), m_params.hotspot == HotspotMode::Min, [this]() {
        m_params.hotspot = HotspotMode::Min;
        pushParams();
    });
    addChoice(hotspotMenu, hotspotGroup, tr("Min/Max"), m_params.hotspot == HotspotMode::MinMax, [this]() {
        m_params.hotspot = HotspotMode::MinMax;
        pushParams();
    });

    addToggle(viewMenu, tr("&Help Overlay"), m_params.showHelp, [this](bool on) {
        m_params.showHelp = on;
        pushParams();
    });
    viewMenu->addSeparator();
    QAction* fullScreenAction = viewMenu->addAction(tr("Full Screen"));
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, [this](bool on) {
        if (on) {
            showFullScreen();
        } else {
            showNormal();
        }
    });

    QMenu* processingMenu = menuBar()->addMenu(tr("&Processing"));

    QMenu* agcMenu = processingMenu->addMenu(tr("&AGC"));
    auto* agcGroup = new QActionGroup(this);
    addChoice(agcMenu, agcGroup, tr("Factory"), m_params.agcMode == AgcMode::Factory, [this]() {
        m_params.agcMode = AgcMode::Factory;
        pushParams();
    });
    addChoice(agcMenu, agcGroup, tr("Temporal"), m_params.agcMode == AgcMode::Temporal, [this]() {
        m_params.agcMode = AgcMode::Temporal;
        pushParams();
    });
    addChoice(agcMenu, agcGroup, tr("Fixed Range"), m_params.agcMode == AgcMode::Fixed, [this]() {
        m_params.agcMode = AgcMode::Fixed;
        pushParams();
    });
    agcMenu->addSeparator();
    QAction* fixedRangeAction = agcMenu->addAction(tr("Fixed Range..."));
    connect(fixedRangeAction, &QAction::triggered, this, &MainWindow::showFixedRangeDialog);

    processingMenu->addSeparator();
    addToggle(processingMenu, tr("Enhanced (CLAHE + DDE)"), m_params.useClahe, [this](bool on) {
        m_params.useClahe = on;
        if (on && m_params.ddeStrength <= 0.0) {
            m_params.ddeStrength = 0.3;
        }
        pushParams();
    });
    addToggle(processingMenu, tr("Detail Enhancement (DDE)"), m_params.ddeStrength > 0.0, [this](bool on) {
        m_params.ddeStrength = on ? 0.3 : 0.0;
        pushParams();
    });
    addToggle(processingMenu, tr("Temporal Noise Reduction"), m_params.tnrAlpha < 1.0, [this](bool on) {
        m_params.tnrAlpha = on ? 0.5 : 1.0;
        pushParams();
    });

    processingMenu->addSeparator();
    QMenu* emissivityMenu = processingMenu->addMenu(tr("&Emissivity"));
    for (const double value : {0.95, 0.90, 0.85, 0.80, 0.70, 0.50, 0.30, 0.10}) {
        QAction* action = emissivityMenu->addAction(QString::number(value, 'f', 2));
        connect(action, &QAction::triggered, this, [this, value]() { setEmissivity(value); });
    }
    emissivityMenu->addSeparator();
    QAction* customEmissivityAction = emissivityMenu->addAction(tr("Set..."));
    connect(customEmissivityAction, &QAction::triggered, this, &MainWindow::showEmissivityDialog);

    QMenu* cameraMenu = menuBar()->addMenu(tr("&Camera"));

    QMenu* modelMenu = cameraMenu->addMenu(tr("&Model"));
    auto* modelGroup = new QActionGroup(this);
    addChoice(modelMenu, modelGroup, tr("P1 (160x120)"), m_model == Model::P1, [this]() {
        m_model = Model::P1;
        restartCapture();
    });
    addChoice(modelMenu, modelGroup, tr("P3 (256x192)"), m_model == Model::P3, [this]() {
        m_model = Model::P3;
        restartCapture();
    });

    QAction* reconnectAction = cameraMenu->addAction(tr("&Reconnect"));
    connect(reconnectAction, &QAction::triggered, this, &MainWindow::restartCapture);

    cameraMenu->addSeparator();
    QAction* shutterAction = cameraMenu->addAction(tr("Trigger &Shutter/NUC"));
    connect(shutterAction, &QAction::triggered, this, [this]() {
        if (m_worker != nullptr) {
            m_worker->requestShutter();
        }
    });

    QMenu* gainMenu = cameraMenu->addMenu(tr("&Gain"));
    auto* gainGroup = new QActionGroup(this);
    addChoice(gainMenu, gainGroup, tr("High (sensitive)"), m_params.gain == GainMode::High, [this]() {
        m_params.gain = GainMode::High;
        if (m_worker != nullptr) {
            m_worker->requestGainMode(GainMode::High);
        }
        pushParams();
    });
    addChoice(gainMenu, gainGroup, tr("Low (extended)"), m_params.gain == GainMode::Low, [this]() {
        m_params.gain = GainMode::Low;
        if (m_worker != nullptr) {
            m_worker->requestGainMode(GainMode::Low);
        }
        pushParams();
    });

    cameraMenu->addSeparator();
    QAction* deviceInfoAction = cameraMenu->addAction(tr("Device &Information..."));
    connect(deviceInfoAction, &QAction::triggered, this, &MainWindow::showDeviceInfo);

    QMenu* lockInMenu = menuBar()->addMenu(tr("&Lock-In"));
    m_lockInStartAction = lockInMenu->addAction(tr("&Start"));
    connect(m_lockInStartAction, &QAction::triggered, this, &MainWindow::startLockIn);
    m_lockInStopAction = lockInMenu->addAction(tr("S&top"));
    m_lockInStopAction->setEnabled(false);
    connect(m_lockInStopAction, &QAction::triggered, this, &MainWindow::stopLockIn);
    lockInMenu->addSeparator();
    QAction* configureAction = lockInMenu->addAction(tr("&Configure..."));
    connect(configureAction, &QAction::triggered, this, &MainWindow::showLockInConfig);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* helpAction = helpMenu->addAction(tr("&User Guide"));
    helpAction->setShortcut(QKeySequence::HelpContents);
    connect(helpAction, &QAction::triggered, this, &MainWindow::showHelpDialog);
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(tr("&About QtThermal"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About QtThermal"),
                           tr("<h3>QtThermal</h3>"
                              "<p>Qt6 thermal camera viewer for P3-series USB cameras.</p>"
                              "<p>Version %1</p>")
                               .arg(QStringLiteral(QTTHERMAL_VERSION_STRING)));
    });
    QAction* aboutQtAction = helpMenu->addAction(tr("About &Qt"));
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::buildStatusBar()
{
    m_connectionLabel = new QLabel(tr("Disconnected"), this);
    m_spotLabel = new QLabel(tr("-- \u00b0C"), this);
    m_fpsLabel = new QLabel(tr("0.0 FPS"), this);

    statusBar()->addPermanentWidget(m_connectionLabel);
    statusBar()->addPermanentWidget(m_spotLabel);
    statusBar()->addPermanentWidget(m_fpsLabel);
}

void MainWindow::loadSettings()
{
    QSettings settings;

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty()) {
        restoreGeometry(geometry);
    }
    const QByteArray state = settings.value(QStringLiteral("window/state")).toByteArray();
    if (!state.isEmpty()) {
        restoreState(state);
    }

    if (!m_modelSpecified) {
        Model stored = m_model;
        if (parseModel(settings.value(QStringLiteral("camera/model"), QStringLiteral("p3"))
                           .toString()
                           .toStdString(),
                       stored)) {
            m_model = stored;
        }
    }

    const auto readInt = [&settings](const QString& key, int defaultValue, int minimum, int maximum) {
        const int value = settings.value(key, defaultValue).toInt();
        if (value < minimum) {
            return minimum;
        }
        if (value > maximum) {
            return maximum;
        }
        return value;
    };

    m_params.colormap = static_cast<Colormap>(readInt(QStringLiteral("processing/colormap"),
                                                      static_cast<int>(Colormap::Ironbow), 0, 5));
    m_params.agcMode = static_cast<AgcMode>(
        readInt(QStringLiteral("processing/agc"), static_cast<int>(AgcMode::Factory), 0, 2));
    m_params.scaleMode = static_cast<ScaleMode>(
        readInt(QStringLiteral("processing/scale"), static_cast<int>(ScaleMode::Bicubic), 0, 4));
    m_params.hotspot = static_cast<HotspotMode>(
        readInt(QStringLiteral("processing/hotspot"), static_cast<int>(HotspotMode::Off), 0, 3));

    const int rotation = readInt(QStringLiteral("processing/rotation"), 0, 0, 359);
    m_params.rotation = (rotation % 90 == 0) ? rotation : 0;
    m_params.zoom = readInt(QStringLiteral("processing/zoom"), 3, 1, 5);
    m_params.useClahe = settings.value(QStringLiteral("processing/clahe"), true).toBool();
    m_params.ddeStrength = settings.value(QStringLiteral("processing/dde"), 0.3).toDouble();
    m_params.tnrAlpha = settings.value(QStringLiteral("processing/tnr"), 0.5).toDouble();
    m_params.mirror = settings.value(QStringLiteral("processing/mirror"), false).toBool();
    m_params.showReticule = settings.value(QStringLiteral("processing/reticule"), true).toBool();
    m_params.showColorbar = settings.value(QStringLiteral("processing/colorbar"), true).toBool();
    m_params.showHelp = settings.value(QStringLiteral("processing/help"), false).toBool();
    m_params.fixedRangeMin =
        qBound(-20.0, settings.value(QStringLiteral("processing/fixedMin"), 10.0).toDouble(), 600.0);
    m_params.fixedRangeMax =
        qBound(-20.0, settings.value(QStringLiteral("processing/fixedMax"), 40.0).toDouble(), 600.0);
    if (m_params.fixedRangeMax <= m_params.fixedRangeMin) {
        m_params.fixedRangeMin = 10.0;
        m_params.fixedRangeMax = 40.0;
    }
    m_params.env.emissivity = settings.value(QStringLiteral("processing/emissivity"), 0.95).toDouble();
    m_params.env.reflectedTemp =
        settings.value(QStringLiteral("processing/reflectedTemp"), 25.0).toDouble();
    m_params.gain = static_cast<GainMode>(
        readInt(QStringLiteral("camera/gain"), static_cast<int>(GainMode::High), 0, 1));

    m_lockInConfig.port =
        settings.value(QStringLiteral("lockin/port"), QString::fromStdString(m_lockInDefaults.port))
            .toString()
            .toStdString();
    m_lockInConfig.baudRate = settings.value(QStringLiteral("lockin/baud"), 115200).toInt();
    m_lockInConfig.period = settings.value(QStringLiteral("lockin/period"), 1.0).toDouble();
    m_lockInConfig.integration = settings.value(QStringLiteral("lockin/integration"), 60.0).toDouble();
    m_lockInConfig.invert = settings.value(QStringLiteral("lockin/invert"), false).toBool();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    settings.setValue(QStringLiteral("camera/model"),
                      QString::fromStdString(modelName(m_model)).toLower());
    settings.setValue(QStringLiteral("camera/gain"), static_cast<int>(m_params.gain));
    settings.setValue(QStringLiteral("processing/colormap"), static_cast<int>(m_params.colormap));
    settings.setValue(QStringLiteral("processing/agc"), static_cast<int>(m_params.agcMode));
    settings.setValue(QStringLiteral("processing/scale"), static_cast<int>(m_params.scaleMode));
    settings.setValue(QStringLiteral("processing/hotspot"), static_cast<int>(m_params.hotspot));
    settings.setValue(QStringLiteral("processing/rotation"), m_params.rotation);
    settings.setValue(QStringLiteral("processing/zoom"), m_params.zoom);
    settings.setValue(QStringLiteral("processing/clahe"), m_params.useClahe);
    settings.setValue(QStringLiteral("processing/dde"), m_params.ddeStrength);
    settings.setValue(QStringLiteral("processing/tnr"), m_params.tnrAlpha);
    settings.setValue(QStringLiteral("processing/mirror"), m_params.mirror);
    settings.setValue(QStringLiteral("processing/reticule"), m_params.showReticule);
    settings.setValue(QStringLiteral("processing/colorbar"), m_params.showColorbar);
    settings.setValue(QStringLiteral("processing/help"), m_params.showHelp);
    settings.setValue(QStringLiteral("processing/fixedMin"), m_params.fixedRangeMin);
    settings.setValue(QStringLiteral("processing/fixedMax"), m_params.fixedRangeMax);
    settings.setValue(QStringLiteral("processing/emissivity"), m_params.env.emissivity);
    settings.setValue(QStringLiteral("processing/reflectedTemp"), m_params.env.reflectedTemp);
    settings.setValue(QStringLiteral("lockin/port"), QString::fromStdString(m_lockInConfig.port));
    settings.setValue(QStringLiteral("lockin/baud"), m_lockInConfig.baudRate);
    settings.setValue(QStringLiteral("lockin/period"), m_lockInConfig.period);
    settings.setValue(QStringLiteral("lockin/integration"), m_lockInConfig.integration);
    settings.setValue(QStringLiteral("lockin/invert"), m_lockInConfig.invert);
}

void MainWindow::startCapture()
{
    if (m_worker != nullptr) {
        return;
    }

    setConnectionState(ConnectionState::Connecting);

    m_captureThread = new QThread(this);
    m_worker = new CaptureWorker(m_model, m_simulate);
    m_worker->setParams(m_params);
    if (m_lockIn != nullptr) {
        m_worker->setLockInController(m_lockIn);
    }

    connect(m_captureThread, &QThread::started, m_worker, &CaptureWorker::run);
    connect(m_worker, &CaptureWorker::frameReady, this, &MainWindow::onFrameReady);
    connect(m_worker, &CaptureWorker::statusUpdated, this, &MainWindow::onStatusUpdated);
    connect(m_worker, &CaptureWorker::connectionChanged, this, &MainWindow::onConnectionChanged);
    connect(m_worker, &CaptureWorker::deviceInfoReady, this, &MainWindow::onDeviceInfoReady);
    connect(m_worker, &CaptureWorker::rawFrameReady, this, &MainWindow::onRawFrameReady);
    connect(m_worker, &CaptureWorker::errorOccurred, this, &MainWindow::onErrorOccurred);

    m_worker->moveToThread(m_captureThread);
    m_captureThread->start();
}

void MainWindow::stopCapture()
{
    if (m_worker != nullptr) {
        // Ignore any late signals from the worker that is being torn down.
        m_worker->disconnect(this);
        m_worker->requestStop();
    }
    if (m_captureThread != nullptr) {
        m_captureThread->quit();
        m_captureThread->wait(5000);
    }
    delete m_worker;
    m_worker = nullptr;
    delete m_captureThread;
    m_captureThread = nullptr;

    setConnectionState(ConnectionState::Disconnected);
    m_view->clear();
}

void MainWindow::restartCapture()
{
    stopLockIn();
    stopCapture();

    if (m_lockIn != nullptr) {
        delete m_lockIn;
        m_lockIn = nullptr;
    }

    updateWindowTitle();
    startCapture();
}

void MainWindow::pushParams()
{
    if (m_worker != nullptr) {
        m_worker->setParams(m_params);
    }
}

void MainWindow::showDeviceInfo()
{
    QMessageBox::information(
        this, tr("Device Information"),
        tr("<b>Model:</b> %1<br>"
           "<b>Firmware:</b> %2<br>"
           "<b>Part number:</b> %3<br>"
           "<b>Serial:</b> %4<br>"
           "<b>Hardware:</b> %5<br>"
           "<b>Model (long):</b> %6")
            .arg(m_deviceModel, m_deviceFirmware, m_devicePartNumber, m_deviceSerial,
                 m_deviceHardware, m_deviceModelLong));
}

void MainWindow::showEmissivityDialog()
{
    EmissivityDialog dialog(m_params.env.emissivity, m_params.env.reflectedTemp, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_params.env.emissivity = dialog.emissivity();
        m_params.env.reflectedTemp = dialog.reflectedTemp();
        pushParams();
    }
}

void MainWindow::setEmissivity(double value)
{
    m_params.env.emissivity = value;
    pushParams();
}

void MainWindow::showFixedRangeDialog()
{
    FixedRangeDialog dialog(m_params.fixedRangeMin, m_params.fixedRangeMax, m_sceneRangeValid,
                            m_sceneMinTemp, m_sceneMaxTemp, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    m_params.fixedRangeMin = dialog.minimum();
    m_params.fixedRangeMax = dialog.maximum();
    pushParams();
}

void MainWindow::showLockInConfig()
{
    LockInDialog dialog(m_lockInConfig, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_lockInConfig = dialog.config();
    }
}

void MainWindow::startLockIn()
{
    if (m_lockIn == nullptr) {
        m_lockIn = new LockInController(modelConfig(m_model), this);
        connect(m_lockIn, &LockInController::progress, this, [this](int percent) {
            statusBar()->showMessage(tr("Lock-in: %1%").arg(percent));
        });
        connect(m_lockIn, &LockInController::errorOccurred, this, &MainWindow::onErrorOccurred);
        connect(m_lockIn, &LockInController::finished, this, &MainWindow::onLockInFinished);
    }
    if (m_worker != nullptr) {
        m_worker->setLockInController(m_lockIn);
    }

    QString error;
    if (!m_lockIn->start(m_lockInConfig, &error)) {
        QMessageBox::warning(this, tr("Lock-In"), error);
        return;
    }
    if (m_worker != nullptr) {
        m_worker->setLockInActive(true);
    }
    m_lockInStartAction->setEnabled(false);
    m_lockInStopAction->setEnabled(true);
    statusBar()->showMessage(tr("Lock-in started"));
}

void MainWindow::stopLockIn()
{
    if (m_lockIn != nullptr) {
        m_lockIn->stop(true);
    }
    if (m_worker != nullptr) {
        m_worker->setLockInActive(false);
    }
    if (m_lockInStartAction != nullptr) {
        m_lockInStartAction->setEnabled(true);
    }
    if (m_lockInStopAction != nullptr) {
        m_lockInStopAction->setEnabled(false);
    }
}

void MainWindow::saveScreenshot()
{
    if (m_lastImage.isNull()) {
        statusBar()->showMessage(tr("No frame available"), 5000);
        return;
    }

    const QString suggested =
        QStringLiteral("qtthermal_%1.png")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Save Screenshot"), suggested,
                                                      tr("PNG image (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    if (m_lastImage.save(path)) {
        statusBar()->showMessage(tr("Saved %1").arg(path), 5000);
    } else {
        statusBar()->showMessage(tr("Failed to save %1").arg(path), 5000);
    }
}

void MainWindow::requestRawDump()
{
    if (m_worker == nullptr) {
        return;
    }

    const QString suggested =
        QStringLiteral("qtthermal_%1.pgm")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Dump Raw Data"), suggested,
                                                      tr("Portable Gray Map (*.pgm)"));
    if (path.isEmpty()) {
        return;
    }
    m_pendingDumpPath = path;
    m_worker->requestRawDump();
}

void MainWindow::showHelpDialog()
{
    if (m_helpDialog == nullptr) {
        m_helpDialog = new HelpDialog(this);
    }
    m_helpDialog->show();
    m_helpDialog->raise();
    m_helpDialog->activateWindow();
}

void MainWindow::updateWindowTitle()
{
    const QString model = QString::fromStdString(modelName(m_model));
    setWindowTitle(m_simulate ? tr("QtThermal - %1 (simulated)").arg(model)
                              : tr("QtThermal - %1").arg(model));
}

} // namespace qtthermal
