#include "CalibrationOverlay.hpp"
#include "../infrastructure/Logger.hpp"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

CalibrationOverlay::CalibrationOverlay(Callback cb, QWidget* parent)
    : QWidget(parent,
              Qt::Window |
              Qt::FramelessWindowHint |
              Qt::WindowStaysOnTopHint |
              Qt::Tool)
    , m_cb(std::move(cb))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setWindowTitle("ChessBotX - Calibration");
    setCursor(Qt::CrossCursor);
    setFocusPolicy(Qt::StrongFocus);

    QRect screen = QApplication::primaryScreen()->geometry();
    setGeometry(screen);

    LOG_INFO("Transparent calibration overlay opened: screen=({},{} {}x{})",
             screen.x(), screen.y(), screen.width(), screen.height());

    // Do not use showFullScreen() on GNOME/Wayland: fullscreen translucent
    // surfaces are often composited as opaque black. A frameless top-level
    // window with screen geometry gives the same UX without triggering that path.
    show();
    raise();
    activateWindow();
}

void CalibrationOverlay::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setCompositionMode(QPainter::CompositionMode_Clear);
    p.fillRect(rect(), Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    QRect banner(24, 24, qMin(width() - 48, 780), 64);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 135));
    p.drawRoundedRect(banner, 6, 6);

    p.setPen(QColor(245, 245, 245));
    p.setFont(QFont("Arial", 13, QFont::Bold));

    if (!m_firstDone) {
        p.drawText(banner, Qt::AlignCenter,
                   "Calibration: clique le coin HAUT-GAUCHE du plateau   |   Esc annule");
        return;
    }

    QPoint localTopLeft = mapFromGlobal(m_topLeft);
    p.setPen(QPen(QColor(80, 255, 120), 3));
    p.drawLine(localTopLeft.x() - 22, localTopLeft.y(), localTopLeft.x() + 22, localTopLeft.y());
    p.drawLine(localTopLeft.x(), localTopLeft.y() - 22, localTopLeft.x(), localTopLeft.y() + 22);
    p.drawEllipse(QRect(localTopLeft.x() - 9, localTopLeft.y() - 9, 18, 18));

    p.setPen(QColor(245, 245, 245));
    p.setFont(QFont("Arial", 13, QFont::Bold));
    p.drawText(banner, Qt::AlignCenter,
               "Calibration: clique maintenant le coin BAS-DROIT du plateau   |   Esc annule");
}

void CalibrationOverlay::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton)
        return;

    QPoint globalPoint = e->globalPosition().toPoint();
    if (!m_firstDone) {
        m_topLeft = globalPoint;
        m_firstDone = true;
        LOG_INFO("Calibration first click: global=({}, {})", m_topLeft.x(), m_topLeft.y());
        update();
        return;
    }

    QPoint bottomRight = globalPoint;
    LOG_INFO("Calibration second click: global=({}, {}), final rect=({},{} {}x{})",
             bottomRight.x(), bottomRight.y(),
             qMin(m_topLeft.x(), bottomRight.x()),
             qMin(m_topLeft.y(), bottomRight.y()),
             qAbs(bottomRight.x() - m_topLeft.x()),
             qAbs(bottomRight.y() - m_topLeft.y()));

    close();
    if (m_cb)
        m_cb(m_topLeft, bottomRight);
}

void CalibrationOverlay::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) {
        LOG_INFO("Calibration cancelled");
        close();
    }
}
