#pragma once

#include "camera/CaptureWorker.h"
#include "camera/CameraBackend.h"
#include "core/ImageProcessor.h"
#include "lockin/LockInController.h"

#include <QImage>
#include <QMainWindow>
#include <QString>

class QAction;
class QLabel;
class QThread;

namespace qtthermal {

class ThermalView;

/// Main application window: menus, status bar and the thermal view.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(Model model, bool simulate, bool modelSpecified, QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onFrameReady(const QImage& image);
    void onStatusUpdated(double fps, double spotTemp, double minTemp, double maxTemp);
    void onConnectionChanged(bool connected);
    void onDeviceInfoReady(const QString& model, const QString& firmware, const QString& partNumber,
                           const QString& serial, const QString& hardware, const QString& modelLong);
    void onErrorOccurred(const QString& message);
    void onRawFrameReady(const QVector<quint16>& data, int width, int height);
    void onLockInFinished();

private:
    void buildMenus();
    void buildStatusBar();
    void loadSettings();
    void saveSettings();

    void startCapture();
    void stopCapture();
    void restartCapture();
    void pushParams();

    void showDeviceInfo();
    void showEmissivityDialog();
    void setEmissivity(double value);
    void showFixedRangeDialog();
    void showLockInConfig();
    void startLockIn();
    void stopLockIn();
    void saveScreenshot();
    void requestRawDump();

    void updateWindowTitle();

    Model m_model;
    bool m_simulate = false;
    bool m_modelSpecified = false;
    ProcessingParams m_params;
    LockInConfig m_lockInConfig;
    LockInConfig m_lockInDefaults;

    QString m_deviceModel;
    QString m_deviceFirmware;
    QString m_devicePartNumber;
    QString m_deviceSerial;
    QString m_deviceHardware;
    QString m_deviceModelLong;

    ThermalView* m_view = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_spotLabel = nullptr;
    QLabel* m_fpsLabel = nullptr;

    QThread* m_captureThread = nullptr;
    CaptureWorker* m_worker = nullptr;
    LockInController* m_lockIn = nullptr;
    QAction* m_lockInStartAction = nullptr;
    QAction* m_lockInStopAction = nullptr;

    QImage m_lastImage;
    QString m_pendingDumpPath;

    bool m_sceneRangeValid = false;
    double m_sceneMinTemp = 0.0;
    double m_sceneMaxTemp = 0.0;
};

} // namespace qtthermal
