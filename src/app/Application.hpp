#pragma once
#include "../infrastructure/Configuration.hpp"
#include "../engine/StockfishProcess.hpp"
#include "../vision/ScreenCapture.hpp"
#include "../vision/BoardCalibration.hpp"
#include "../vision/BoardDetector.hpp"
#include "../vision/FrameStabilityDetector.hpp"
#include "../chess/PositionValidator.hpp"
#include "../ui/OverlayWidget.hpp"
#include "../ui/SettingsWindow.hpp"
#include "../ui/StatusWindow.hpp"
#include <QObject>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <memory>
#include <vector>

class Application : public QObject {
    Q_OBJECT
public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;

    bool init();
    void startAnalysis();
    void stopAnalysis();
    void toggleAutoPlay();

private slots:
    void onTick();
    void onBestMove(const UciBestMove& bm);
    void onInfo(const UciInfo& info);
    void onSettingsChanged();
    void showSettings();
    void onCalibrate();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onCalibrationDone(QPoint topLeft, QPoint bottomRight);
    void onResetCalibration();

private:
    void setupTray();
    void setupHotkeys();
    void applyEngineOptions();
    void playMove(const UciMove& move);
    void applyCalibration();

    std::unique_ptr<Configuration>          m_config;
    std::unique_ptr<StockfishProcess>       m_engine;
    std::unique_ptr<ScreenCapture>          m_capture;
    std::unique_ptr<BoardCalibration>       m_calibration;
    std::unique_ptr<BoardDetector>          m_detector;
    std::unique_ptr<FrameStabilityDetector> m_stability;
    std::unique_ptr<PositionValidator>      m_validator;
    std::unique_ptr<OverlayWidget>          m_overlay;
    std::unique_ptr<SettingsWindow>         m_settingsWin;
    std::unique_ptr<StatusWindow>           m_status;
    std::unique_ptr<QSystemTrayIcon>        m_tray;
    std::unique_ptr<QMenu>                  m_trayMenu;

    QTimer* m_ticker = nullptr;

    std::vector<UciInfo> m_lastInfos;
    bool m_analyzing    = false;
    bool m_autoPlay     = false;
    bool m_waitingBest  = false;
    int  m_failCount    = 0;
    int  m_tickCount    = 0;
    int  m_captureFailCount = 0;
    int  m_waitingBestTicks = 0;
    int  m_notCalibratedTicks = 0;
    static constexpr int kMaxFails = 5;
    static constexpr int kTickMs   = 300;
};
