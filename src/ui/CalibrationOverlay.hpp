#pragma once
#include <QWidget>
#include <QPoint>
#include <functional>

// Full-screen transparent two-click calibration overlay.
class CalibrationOverlay : public QWidget {
    Q_OBJECT
public:
    using Callback = std::function<void(QPoint, QPoint)>;

    explicit CalibrationOverlay(Callback cb, QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    Callback m_cb;
    QPoint   m_topLeft;
    bool     m_firstDone = false;
};
