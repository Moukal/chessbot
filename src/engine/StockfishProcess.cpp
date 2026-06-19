#include "StockfishProcess.hpp"
#include "../infrastructure/Logger.hpp"
#include <QElapsedTimer>

StockfishProcess::StockfishProcess(const QString& enginePath, QObject* parent)
    : QObject(parent), m_enginePath(enginePath)
{
    m_restartTimer = new QTimer(this);
    m_restartTimer->setSingleShot(true);
    connect(m_restartTimer, &QTimer::timeout, this, [this]{ start(); });

    m_searchTimeout = new QTimer(this);
    m_searchTimeout->setSingleShot(true);
    connect(m_searchTimeout, &QTimer::timeout, this, [this]{
        if (m_searching) {
            LOG_WARN("Search timeout — sending stop");
            stopSearch();
        }
    });
}

StockfishProcess::~StockfishProcess() {
    stop();
}

bool StockfishProcess::start() {
    if (m_process && m_process->state() != QProcess::NotRunning)
        stop();

    m_process = new QProcess(this);
    connect(m_process, &QProcess::errorOccurred, this, &StockfishProcess::onProcessError);
    connect(m_process, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &StockfishProcess::onProcessFinished);

    m_process->start(m_enginePath, QStringList{});
    if (!m_process->waitForStarted(3000)) {
        LOG_ERROR("Failed to start engine: {}", m_enginePath.toStdString());
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }

    send("uci");
    if (!waitFor("uciok", 5000)) {
        LOG_ERROR("Engine did not respond with uciok");
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }

    send("isready");
    if (!waitFor("readyok", 5000)) {
        LOG_ERROR("Engine did not respond with readyok");
        m_process->kill();
        m_process->deleteLater();
        m_process = nullptr;
        return false;
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &StockfishProcess::onReadyRead);
    m_ready = true;
    m_restartCount = 0;
    LOG_INFO("Engine started: {}", m_enginePath.toStdString());
    return true;
}

void StockfishProcess::stop() {
    if (!m_process) return;
    m_ready = false;
    send("quit");
    if (!m_process->waitForFinished(2000))
        m_process->kill();
    m_process->deleteLater();
    m_process = nullptr;
}

bool StockfishProcess::isRunning() const {
    return m_process && m_process->state() == QProcess::Running && m_ready;
}

void StockfishProcess::setPosition(const std::string& fen) {
    if (!isRunning()) return;
    send(QString("position fen %1").arg(QString::fromStdString(fen)));
}

void StockfishProcess::go(int moveTimeMs, int multiPV) {
    if (!isRunning() || m_searching) return;
    setOption("MultiPV", std::to_string(multiPV));
    send(QString("go movetime %1").arg(moveTimeMs));
    m_searching = true;
    m_searchTimeout->start(moveTimeMs + 3000);
}

void StockfishProcess::stopSearch() {
    if (!isRunning()) return;
    send("stop");
    m_searching = false;
    m_searchTimeout->stop();
}

void StockfishProcess::setOption(const std::string& name, const std::string& value) {
    if (!m_process || m_process->state() != QProcess::Running) return;
    send(QString("setoption name %1 value %2")
         .arg(QString::fromStdString(name))
         .arg(QString::fromStdString(value)));
}

void StockfishProcess::send(const QString& cmd) {
    if (!m_process) return;
    LOG_DEBUG(">> {}", cmd.toStdString());
    m_process->write((cmd + "\n").toUtf8());
}

void StockfishProcess::processLine(const QString& line) {
    const std::string s = line.trimmed().toStdString();
    if (s.empty()) return;
    LOG_TRACE("<< {}", s);

    if (auto info = UciParser::parseInfo(s); info && m_infoCallback)
        m_infoCallback(*info);
    else if (auto bm = UciParser::parseBestMove(s)) {
        m_searching = false;
        m_searchTimeout->stop();
        if (m_bestMoveCallback) m_bestMoveCallback(*bm);
    }
}

void StockfishProcess::onReadyRead() {
    if (!m_process) return;
    while (m_process->canReadLine()) {
        const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
        processLine(line);
    }
}

bool StockfishProcess::waitFor(const std::string& token, int timeoutMs) {
    QElapsedTimer elapsed;
    elapsed.start();

    while (elapsed.elapsed() < timeoutMs) {
        if (!m_process->waitForReadyRead(500))
            continue;
        while (m_process->canReadLine()) {
            const QString line = QString::fromUtf8(m_process->readLine()).trimmed();
            const std::string s = line.toStdString();
            LOG_TRACE("<< {}", s);
            if (s == token)
                return true;
        }
    }
    LOG_ERROR("Timeout waiting for '{}'", token);
    return false;
}

void StockfishProcess::onProcessError(QProcess::ProcessError error) {
    LOG_ERROR("Engine process error: {}", static_cast<int>(error));
}

void StockfishProcess::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    m_ready = false;
    LOG_WARN("Engine process finished (exit={}, status={})", exitCode, static_cast<int>(status));

    if (m_restartCount < kMaxRestarts) {
        ++m_restartCount;
        LOG_INFO("Restarting engine in 2s (attempt {}/{})", m_restartCount, kMaxRestarts);
        m_restartTimer->start(2000);
    } else {
        LOG_ERROR("Engine failed to restart after {} attempts", kMaxRestarts);
    }
}
