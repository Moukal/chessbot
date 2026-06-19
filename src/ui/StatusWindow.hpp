#pragma once
#include <QWidget>
#include <QLabel>
#include <QPoint>
#include <QTextEdit>

class StatusWindow : public QWidget {
    Q_OBJECT
public:
    explicit StatusWindow(QWidget* parent = nullptr);

    void setEngine(bool ok);
    void setAnalyzing(bool on);
    void setFen(const QString& fen);
    void setBestMove(const QString& move, int scoreCp);
    void appendLog(const QString& line);

protected:
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;

private:
    void refresh();

    QLabel* m_label;
    QTextEdit* m_logs;
    QPoint  m_dragPos;

    bool    m_engineOk   = false;
    bool    m_analyzing  = false;
    QString m_fen;
    QString m_bestMove;
    int     m_score      = 0;
};
