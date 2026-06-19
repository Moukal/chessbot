#include "StatusWindow.hpp"
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QApplication>
#include <QScreen>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

StatusWindow::StatusWindow(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setWindowTitle("ChessBotX Diagnostics");
    resize(760, 420);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_label->setWordWrap(false);
    m_label->setStyleSheet(
        "background: #151515;"
        "color: #00ff88;"
        "font-family: monospace;"
        "font-size: 11px;"
        "padding: 8px 12px;"
        "border-bottom: 1px solid #303030;"
    );

    m_logs = new QTextEdit(this);
    m_logs->setReadOnly(true);
    m_logs->setLineWrapMode(QTextEdit::NoWrap);
    m_logs->setStyleSheet(
        "QTextEdit {"
        "background: #0b0b0b;"
        "color: #d6d6d6;"
        "font-family: monospace;"
        "font-size: 11px;"
        "border: 0;"
        "padding: 6px;"
        "}"
    );

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addWidget(m_label);
    lay->addWidget(m_logs, 1);

    // Position: top-right corner
    QRect screen = QApplication::primaryScreen()->geometry();
    move(screen.right() - width() - 24, screen.top() + 40);

    refresh();
}

void StatusWindow::setEngine(bool ok) { m_engineOk = ok; refresh(); }
void StatusWindow::setAnalyzing(bool on) { m_analyzing = on; refresh(); }
void StatusWindow::setFen(const QString& fen) { m_fen = fen; refresh(); }
void StatusWindow::setBestMove(const QString& move, int scoreCp) {
    m_bestMove = move;
    m_score = scoreCp;
    refresh();
}

void StatusWindow::appendLog(const QString& line) {
    QString text = line.trimmed().toHtmlEscaped();
    if (text.contains("[error]")) {
        text = QString("<span style='color:#ff5f57'>%1</span>").arg(text);
    } else if (text.contains("[warning]")) {
        text = QString("<span style='color:#ffbd2e'>%1</span>").arg(text);
    } else if (text.contains("[info]")) {
        text = QString("<span style='color:#8fdfff'>%1</span>").arg(text);
    } else if (text.contains("[debug]")) {
        text = QString("<span style='color:#d6d6d6'>%1</span>").arg(text);
    } else {
        text = QString("<span style='color:#8a8a8a'>%1</span>").arg(text);
    }

    m_logs->append(text);

    QTextDocument* doc = m_logs->document();
    constexpr int kMaxLogBlocks = 350;
    while (doc->blockCount() > kMaxLogBlocks) {
        QTextCursor cursor(doc->firstBlock());
        cursor.select(QTextCursor::BlockUnderCursor);
        cursor.removeSelectedText();
        cursor.deleteChar();
    }

    m_logs->verticalScrollBar()->setValue(m_logs->verticalScrollBar()->maximum());
}

void StatusWindow::refresh() {
    QString eng   = m_engineOk  ? "<span style='color:#00ff88'>● Engine OK</span>"
                                : "<span style='color:#ff4444'>● Engine ERREUR</span>";
    QString ana   = m_analyzing ? "<span style='color:#00ff88'>● Analyse ON</span>"
                                : "<span style='color:#aaaaaa'>○ Analyse OFF</span>";
    QString fen   = m_fen.isEmpty() ? "<span style='color:#888'>FEN: en attente...</span>"
                                    : QString("<span style='color:#fff'>FEN: %1</span>")
                                        .arg(m_fen.left(30) + "...");
    QString best  = m_bestMove.isEmpty() ? "<span style='color:#888'>Coup: -</span>"
                                         : QString("<span style='color:#ffcc00'>Coup: %1 (%2cp)</span>")
                                             .arg(m_bestMove).arg(m_score);

    m_label->setText(QString(
        "<b style='color:#4fc3f7'>ChessBotX</b><br>"
        "%1<br>%2<br>%3<br>%4"
    ).arg(eng, ana, fen, best));
}

void StatusWindow::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton)
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
}

void StatusWindow::mouseMoveEvent(QMouseEvent* e) {
    if (e->buttons() & Qt::LeftButton)
        move(e->globalPosition().toPoint() - m_dragPos);
}
