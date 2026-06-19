#include "OverlayWidget.hpp"
#include "../infrastructure/Logger.hpp"
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QApplication>
#include <cmath>

const QColor OverlayWidget::kColor1 = QColor(0, 200, 0, 200);
const QColor OverlayWidget::kColor2 = QColor(220, 220, 0, 180);
const QColor OverlayWidget::kColor3 = QColor(220, 140, 0, 160);

OverlayWidget::OverlayWidget(QWidget* parent)
    : QWidget(parent,
              Qt::Window |
              Qt::FramelessWindowHint |
              Qt::WindowTransparentForInput |
              Qt::WindowStaysOnTopHint |
              Qt::Tool)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setWindowOpacity(1.0);

    if (auto* screen = QApplication::primaryScreen()) {
        setGeometry(screen->geometry());
    }
}

void OverlayWidget::setCalibration(const CalibrationData& cal) {
    m_cal = cal;
    m_hasCalibration = cal.calibrated;
    update();
}

void OverlayWidget::clear() {
    m_arrows.clear();
    update();
}

void OverlayWidget::setMoves(const std::vector<UciInfo>& infos) {
    m_arrows.clear();

    static const QColor colors[] = {kColor1, kColor2, kColor3};
    int idx = 0;
    for (const auto& info : infos) {
        if (info.pv.empty() || idx >= 3) break;
        if (info.pv[0].from.size() < 2 || info.pv[0].to.size() < 2) { ++idx; continue; }

        QPoint from = squareCenter(info.pv[0].from);
        QPoint to   = squareCenter(info.pv[0].to);

        Arrow a;
        a.from  = from;
        a.to    = to;
        a.color = colors[idx];
        a.width = (idx == 0) ? 8.0f : 5.0f;
        m_arrows.push_back(a);
        ++idx;
    }
    update();
}

QPoint OverlayWidget::squareCenter(const std::string& sq) const {
    if (!m_hasCalibration || sq.size() < 2) return {};
    int file = sq[0] - 'a';
    int rank = sq[1] - '1';
    if (file < 0 || file > 7 || rank < 0 || rank > 7) return {};
    return m_cal.squareCenter(file, rank);
}

void OverlayWidget::paintEvent(QPaintEvent*) {
    if (m_arrows.empty()) return;

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    for (const auto& arrow : m_arrows) {
        if (arrow.from.isNull() || arrow.to.isNull()) continue;

        QPointF from(arrow.from);
        QPointF to(arrow.to);

        double dx = to.x() - from.x();
        double dy = to.y() - from.y();
        double len = std::sqrt(dx*dx + dy*dy);
        if (len < 1.0) continue;

        double ux = dx / len;
        double uy = dy / len;

        // Shorten arrow to not overlap piece
        double shorten = arrow.width * 2.5;
        QPointF start = from + QPointF(ux * shorten, uy * shorten);
        QPointF end   = to   - QPointF(ux * shorten, uy * shorten);

        // Arrow head
        double headLen   = arrow.width * 3.0;
        double headWidth = arrow.width * 2.0;
        QPointF left  = end + QPointF(-uy * headWidth - ux * headLen,
                                       ux * headWidth - uy * headLen);
        QPointF right = end + QPointF( uy * headWidth - ux * headLen,
                                      -ux * headWidth - uy * headLen);

        QPen pen(arrow.color, arrow.width, Qt::SolidLine, Qt::RoundCap);
        p.setPen(pen);
        p.drawLine(start, end);

        QPainterPath head;
        head.moveTo(end);
        head.lineTo(left);
        head.lineTo(right);
        head.closeSubpath();
        p.fillPath(head, arrow.color);
    }
}

void OverlayWidget::setVisible(bool v) {
    QWidget::setVisible(v);
}

void OverlayWidget::toggleVisibility() {
    setVisible(!isVisible());
}
