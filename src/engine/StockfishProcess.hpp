#pragma once
#include "IUciEngine.hpp"
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QString>
#include <memory>

class StockfishProcess : public QObject, public IUciEngine {
    Q_OBJECT
public:
    explicit StockfishProcess(const QString& enginePath, QObject* parent = nullptr);
    ~StockfishProcess() override;

    bool start() override;
    void stop()  override;
    bool isRunning() const override;

    void setPosition(const std::string& fen) override;
    void go(int moveTimeMs, int multiPV = 3) override;
    void stopSearch() override;
    void setOption(const std::string& name, const std::string& value) override;

    void onInfo(InfoCallback cb)         override { m_infoCallback = cb; }
    void onBestMove(BestMoveCallback cb) override { m_bestMoveCallback = cb; }

private slots:
    void onReadyRead();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    void send(const QString& cmd);
    void processLine(const QString& line);
    bool waitFor(const std::string& token, int timeoutMs = 5000);

    QString          m_enginePath;
    QProcess*        m_process = nullptr;
    QTimer*          m_restartTimer = nullptr;
    QTimer*          m_searchTimeout = nullptr;
    InfoCallback     m_infoCallback;
    BestMoveCallback m_bestMoveCallback;
    bool             m_ready      = false;
    bool             m_searching  = false;
    int              m_restartCount = 0;
    static constexpr int kMaxRestarts = 3;
};
