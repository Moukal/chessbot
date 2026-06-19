#include "Application.hpp"
#include "../infrastructure/Logger.hpp"
#include "../ui/CalibrationOverlay.hpp"
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QMessageBox>
#include <QMetaObject>
#include <QRandomGenerator>
#include <cctype>
#include <random>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/extensions/XTest.h>
#undef Bool
#undef None
#undef Status
#endif

static void simulateClick(int x, int y) {
#ifdef Q_OS_LINUX
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) return;
    XTestFakeMotionEvent(dpy, -1, x, y, 0);
    XFlush(dpy);
    XTestFakeButtonEvent(dpy, 1, True,  10);
    XTestFakeButtonEvent(dpy, 1, False, 80);
    XFlush(dpy);
    XCloseDisplay(dpy);
#endif
}

Application::Application(QObject* parent)
    : QObject(parent)
{}

Application::~Application() {
    Logger::setCallback({});
    stopAnalysis();
}

bool Application::init() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(configDir);
    QString configFile = configDir + "/config.json";

    m_config      = std::make_unique<Configuration>(configFile.toStdString());
    m_config->load();

    m_capture     = std::make_unique<ScreenCapture>();
    m_calibration = std::make_unique<BoardCalibration>();
    m_stability   = std::make_unique<FrameStabilityDetector>(3, 0.02);
    m_validator   = std::make_unique<PositionValidator>();
    m_overlay     = std::make_unique<OverlayWidget>();
    m_status      = std::make_unique<StatusWindow>();
    StatusWindow* statusWindow = m_status.get();
    Logger::setCallback([statusWindow](const std::string& line) {
        QString qline = QString::fromStdString(line);
        QMetaObject::invokeMethod(statusWindow, [statusWindow, qline]() {
            statusWindow->appendLog(qline);
        }, Qt::QueuedConnection);
    });
    LOG_INFO("Diagnostics window attached to logger");

    m_detector = std::make_unique<BoardDetector>(m_calibration->data());

    auto configuredRect = m_config->boardRect();
    LOG_INFO("Runtime config: enginePath='{}' playAsWhite={} boardRect=({},{} {}x{})",
             m_config->enginePath(),
             m_config->playAsWhite() ? "true" : "false",
             configuredRect.value("x", 0),
             configuredRect.value("y", 0),
             configuredRect.value("w", 0),
             configuredRect.value("h", 0));

    // Load calibration if saved
    if (configuredRect["w"].get<int>() > 0) {
        applyCalibration();
    } else {
        LOG_WARN("No saved board calibration; analysis will wait until Calibrate Board is completed");
    }
    QString templateDir = QApplication::applicationDirPath() + "/templates";
    m_detector->setTemplateDir(templateDir.toStdString());
    LOG_INFO("Template directory '{}' loaded={}", templateDir.toStdString(), m_detector->hasTemplates() ? "true" : "false");

    m_engine = std::make_unique<StockfishProcess>(
        QString::fromStdString(m_config->enginePath()));

    m_engine->onInfo([this](const UciInfo& info) { onInfo(info); });
    m_engine->onBestMove([this](const UciBestMove& bm) { onBestMove(bm); });

    if (!m_engine->start()) {
        LOG_ERROR("Failed to start engine");
        m_status->setEngine(false);
        QMessageBox::critical(nullptr, "Engine Error",
            "Could not start Stockfish.\nCheck engine path in Settings.");
    } else {
        m_status->setEngine(true);
        applyEngineOptions();
    }

    m_ticker = new QTimer(this);
    connect(m_ticker, &QTimer::timeout, this, &Application::onTick);

    setupTray();
    m_overlay->show();
    m_status->show();
    startAnalysis();

    LOG_INFO("Application initialized");
    return true;
}

void Application::applyEngineOptions() {
    m_engine->setOption("Threads",    std::to_string(m_config->engineThreads()));
    m_engine->setOption("Hash",       std::to_string(m_config->engineHashMb()));
    m_engine->setOption("Skill Level",std::to_string(m_config->engineSkillLevel()));
}

void Application::startAnalysis() {
    if (m_analyzing) return;
    m_analyzing = true;
    m_stability->reset();
    m_failCount = 0;
    m_tickCount = 0;
    m_captureFailCount = 0;
    m_waitingBestTicks = 0;
    m_notCalibratedTicks = 0;
    m_ticker->start(kTickMs);
    m_status->setAnalyzing(true);
    LOG_INFO("Analysis started: calibrated={} tickMs={} hasTemplates={} engineRunning={}",
             m_calibration->isCalibrated() ? "true" : "false",
             kTickMs,
             m_detector->hasTemplates() ? "true" : "false",
             (m_engine && m_engine->isRunning()) ? "true" : "false");
}

void Application::stopAnalysis() {
    if (!m_analyzing) return;
    m_analyzing = false;
    m_ticker->stop();
    if (m_engine) m_engine->stopSearch();
    m_overlay->clear();
    m_status->setAnalyzing(false);
    LOG_INFO("Analysis stopped");
}

void Application::onTick() {
    ++m_tickCount;

    if (!m_calibration->isCalibrated()) {
        ++m_notCalibratedTicks;
        if (m_notCalibratedTicks == 1 || m_notCalibratedTicks % 20 == 0) {
            LOG_WARN("Analysis tick {} skipped: board is not calibrated", m_tickCount);
        }
        return;
    }

    if (m_waitingBest) {
        ++m_waitingBestTicks;
        if (m_waitingBestTicks == 1 || m_waitingBestTicks % 20 == 0) {
            LOG_WARN("Analysis tick {} skipped: waiting for Stockfish bestmove ({} ticks)",
                     m_tickCount, m_waitingBestTicks);
        }
        return;
    }
    m_waitingBestTicks = 0;

    QImage screen = m_capture->grabFullScreen();
    if (screen.isNull()) {
        ++m_captureFailCount;
        LOG_ERROR("Analysis tick {} failed: screen capture is null ({} consecutive capture failures)",
                  m_tickCount, m_captureFailCount);
        return;
    }
    if (m_captureFailCount > 0) {
        LOG_INFO("Screen capture recovered after {} failures", m_captureFailCount);
        m_captureFailCount = 0;
    }

    const auto cal = m_calibration->data();
    LOG_TRACE("Analysis tick {}: screen={}x{} boardRect=({},{} {}x{}) playAsWhite={}",
              m_tickCount,
              screen.width(), screen.height(),
              cal.boardRect.x(), cal.boardRect.y(), cal.boardRect.width(), cal.boardRect.height(),
              cal.playAsWhite ? "true" : "false");

    if (!screen.rect().contains(cal.boardRect)) {
        LOG_ERROR("Analysis tick {} failed: boardRect=({},{} {}x{}) is outside captured screen={}x{}",
                  m_tickCount,
                  cal.boardRect.x(), cal.boardRect.y(), cal.boardRect.width(), cal.boardRect.height(),
                  screen.width(), screen.height());
        return;
    }

    if (!m_stability->feed(screen.copy(cal.boardRect))) {
        if (m_tickCount <= 5 || m_tickCount % 10 == 0) {
            LOG_DEBUG("Analysis tick {} waiting for stable board: diff={} stableFrames={}/{} threshold={}",
                      m_tickCount,
                      m_stability->lastDiff(),
                      m_stability->stableCount(),
                      m_stability->requiredStableFrames(),
                      m_stability->threshold());
        }
        return;
    }

    bool whiteToMove = m_config->playAsWhite();
    std::string fen = m_detector->detectFen(screen, whiteToMove);
    if (fen.empty()) {
        ++m_failCount;
        LOG_WARN("Analysis tick {}: FEN detection returned empty ({}/{})",
                 m_tickCount, m_failCount, kMaxFails);
        if (m_failCount >= kMaxFails) {
            m_stability->reset();
            m_failCount = 0;
        }
        return;
    }

    // Validate FEN: must have exactly 2 kings, between 2 and 32 pieces
    // Only count the piece-placement field (before the first space) to avoid
    // counting 'b'/'w' from the active-color field as pieces.
    int wK = 0, bK = 0, total = 0;
    const std::string_view piecePart(fen.data(),
        fen.find(' ') == std::string::npos ? fen.size() : fen.find(' '));
    for (char c : piecePart) {
        if (c == 'K') { ++wK; ++total; }
        else if (c == 'k') { ++bK; ++total; }
        else if (std::isalpha(static_cast<unsigned char>(c))) ++total;
    }
    if (wK != 1 || bK != 1 || total < 2 || total > 32) {
        LOG_WARN("Analysis tick {}: invalid FEN rejected (wK={} bK={} total={}): {}",
                 m_tickCount, wK, bK, total, fen);
        m_stability->reset();
        return;
    }

    // Validate position transition
    if (!m_validator->currentFen().empty()) {
        auto transition = m_validator->validateTransition(fen);
        if (!transition.has_value()) {
            ++m_failCount;
            LOG_WARN("Analysis tick {}: invalid position transition ({}/{}), fen={}",
                     m_tickCount, m_failCount, kMaxFails, fen);
            if (m_failCount >= kMaxFails) {
                m_validator->reset();
                m_stability->reset();
                m_failCount = 0;
            }
            return;
        }
    }

    m_validator->setCurrentFen(fen);
    m_status->setFen(QString::fromStdString(fen));
    LOG_INFO("Analysis tick {} accepted FEN: {}", m_tickCount, fen);
    m_failCount = 0;
    m_lastInfos.clear();
    m_waitingBest = true;
    m_engine->setPosition(fen);
    m_engine->go(m_config->moveTimeMs(), m_config->multiPV());
}

void Application::onInfo(const UciInfo& info) {
    // Accumulate multiPV lines
    bool found = false;
    for (auto& existing : m_lastInfos) {
        if (existing.multiPvIndex == info.multiPvIndex) {
            existing = info;
            found = true;
            break;
        }
    }
    if (!found) m_lastInfos.push_back(info);

    // Sort by PV index
    std::sort(m_lastInfos.begin(), m_lastInfos.end(),
              [](const UciInfo& a, const UciInfo& b){
                  return a.multiPvIndex < b.multiPvIndex;
              });

    m_overlay->setMoves(m_lastInfos);
}

void Application::onBestMove(const UciBestMove& bm) {
    m_waitingBest = false;

    int score = m_lastInfos.empty() ? 0 : m_lastInfos.front().scoreCp;
    m_status->setBestMove(QString::fromStdString(bm.move.from + bm.move.to), score);
    LOG_INFO("Stockfish bestmove received: {}{} scoreCp={} infoLines={}",
             bm.move.from, bm.move.to, score, m_lastInfos.size());

    if (m_autoPlay && m_calibration->isCalibrated()) {
        playMove(bm.move);
    }
}

void Application::playMove(const UciMove& move) {
    if (move.from.empty() || move.to.empty()) return;

    const auto cal = m_calibration->data();
    int fromFile = move.from[0] - 'a';
    int fromRank = move.from[1] - '1';
    int toFile   = move.to[0]   - 'a';
    int toRank   = move.to[1]   - '1';

    QPoint fromPt = cal.squareCenter(fromFile, fromRank);
    QPoint toPt   = cal.squareCenter(toFile,   toRank);

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(
        m_config->mouseDelayMinMs(),
        m_config->mouseDelayMaxMs()
    );
    int delay = dist(rng);

    LOG_INFO("Playing move {} -> {} (delay {}ms)", move.from, move.to, delay);

    simulateClick(fromPt.x(), fromPt.y());
    QTimer::singleShot(delay, this, [toPt]() {
        simulateClick(toPt.x(), toPt.y());
    });
}

void Application::toggleAutoPlay() {
    m_autoPlay = !m_autoPlay;
    LOG_INFO("Auto-play: {}", m_autoPlay ? "ON" : "OFF");
}

void Application::setupTray() {
    m_trayMenu = std::make_unique<QMenu>();

    auto* actStart    = m_trayMenu->addAction("Start Analysis");
    auto* actStop     = m_trayMenu->addAction("Stop Analysis");
    auto* actAutoPlay = m_trayMenu->addAction("Toggle Auto-Play");
    auto* actOverlay  = m_trayMenu->addAction("Toggle Overlay");
    auto* actDiagnostics = m_trayMenu->addAction("Toggle Diagnostics");
    m_trayMenu->addSeparator();
    auto* actCalib    = m_trayMenu->addAction("Calibrate Board");
    auto* actReset    = m_trayMenu->addAction("Reset Calibration");
    auto* actSettings = m_trayMenu->addAction("Settings...");
    m_trayMenu->addSeparator();
    auto* actQuit     = m_trayMenu->addAction("Quit");

    connect(actStart,    &QAction::triggered, this, &Application::startAnalysis);
    connect(actStop,     &QAction::triggered, this, &Application::stopAnalysis);
    connect(actAutoPlay, &QAction::triggered, this, &Application::toggleAutoPlay);
    connect(actOverlay,  &QAction::triggered, m_overlay.get(), &OverlayWidget::toggleVisibility);
    connect(actDiagnostics, &QAction::triggered, this, [this]() {
        m_status->setVisible(!m_status->isVisible());
        if (m_status->isVisible())
            m_status->raise();
    });
    connect(actCalib,    &QAction::triggered, this, &Application::onCalibrate);
    connect(actReset,    &QAction::triggered, this, &Application::onResetCalibration);
    connect(actSettings, &QAction::triggered, this, &Application::showSettings);
    connect(actQuit,     &QAction::triggered, qApp, &QApplication::quit);

    m_tray = std::make_unique<QSystemTrayIcon>(
        QIcon::fromTheme("chess", QIcon(QApplication::applicationDirPath() + "/icon.png")));
    m_tray->setContextMenu(m_trayMenu.get());
    m_tray->setToolTip("ChessBot");
    m_tray->show();

    connect(m_tray.get(), &QSystemTrayIcon::activated,
            this, &Application::onTrayActivated);
}

void Application::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        m_overlay->toggleVisibility();
    }
}

void Application::showSettings() {
    if (!m_settingsWin) {
        m_settingsWin = std::make_unique<SettingsWindow>(*m_config);
        connect(m_settingsWin.get(), &SettingsWindow::settingsChanged,
                this, &Application::onSettingsChanged);
    }
    m_settingsWin->show();
    m_settingsWin->raise();
}

void Application::onSettingsChanged() {
    if (m_engine && m_engine->isRunning())
        applyEngineOptions();
    m_autoPlay = m_config->autoPlay();
    if (m_calibration->isCalibrated())
        applyCalibration();
}

void Application::onCalibrate() {
    bool wasAnalyzing = m_analyzing;
    if (wasAnalyzing) stopAnalysis();

    LOG_INFO("Calibration started");
    // CalibrationOverlay deletes itself on close
    new CalibrationOverlay([this, wasAnalyzing](QPoint tl, QPoint br) {
        onCalibrationDone(tl, br);
        if (wasAnalyzing) startAnalysis();
    });
}

void Application::onCalibrationDone(QPoint topLeft, QPoint bottomRight) {
    int x = qMin(topLeft.x(), bottomRight.x());
    int y = qMin(topLeft.y(), bottomRight.y());
    int w = qAbs(bottomRight.x() - topLeft.x());
    int h = qAbs(bottomRight.y() - topLeft.y());

    if (w < 100 || h < 100) {
        LOG_WARN("Calibration rejected: board too small ({}x{})", w, h);
        QMessageBox::warning(nullptr, "Calibration", "Zone trop petite, recommencez.");
        return;
    }

    nlohmann::json rect;
    rect["x"] = x;
    rect["y"] = y;
    rect["w"] = w;
    rect["h"] = h;
    m_config->setBoardRect(rect);
    m_config->save();

    applyCalibration();
    LOG_INFO("Calibration saved: topLeft=({}, {}) bottomRight=({}, {}) rect=({},{} {}x{}) playAsWhite={}",
             topLeft.x(), topLeft.y(), bottomRight.x(), bottomRight.y(),
             x, y, w, h,
             m_config->playAsWhite() ? "true" : "false");
    QMessageBox::information(nullptr, "Calibration",
        QString("Calibration sauvegardée.\nPlateau: %1×%2 px").arg(w).arg(h));
}

void Application::onResetCalibration() {
    nlohmann::json empty;
    empty["x"] = 0; empty["y"] = 0; empty["w"] = 0; empty["h"] = 0;
    m_config->setBoardRect(empty);
    m_config->save();
    m_calibration = std::make_unique<BoardCalibration>();
    m_overlay->setCalibration(m_calibration->data());
    stopAnalysis();
    LOG_INFO("Calibration reset");
}

void Application::applyCalibration() {
    auto boardRect = m_config->boardRect();
    boardRect["playAsWhite"] = m_config->playAsWhite();
    m_calibration->loadFromJson(boardRect.dump());
    m_detector->setCalibration(m_calibration->data());
    m_overlay->setCalibration(m_calibration->data());
    const auto cal = m_calibration->data();
    LOG_INFO("Calibration applied: calibrated={} rect=({},{} {}x{}) playAsWhite={}",
             cal.calibrated ? "true" : "false",
             cal.boardRect.x(), cal.boardRect.y(), cal.boardRect.width(), cal.boardRect.height(),
             cal.playAsWhite ? "true" : "false");
}
