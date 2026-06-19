#pragma once
#include "../engine/UciParser.hpp"
#include "../vision/BoardCalibration.hpp"
#include <QWidget>
#include <QRect>
#include <QPoint>
#include <vector>

struct Arrow {
    QPoint from;
    QPoint to;
    QColor color;
    float  width = 6.0f;
};

class OverlayWidget : public QWidget {
    Q_OBJECT
public:
    explicit OverlayWidget(QWidget* parent = nullptr);

    void setCalibration(const CalibrationData& cal);
    void setMoves(const std::vector<UciInfo>& infos);
    void clear();

    void setVisible(bool v) override;
    void toggleVisibility();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    Arrow  buildArrow(const UciInfo& info, int rank) const;
    QPoint squareCenter(const std::string& sq) const;

    CalibrationData      m_cal;
    std::vector<Arrow>   m_arrows;
    bool                 m_hasCalibration = false;

    static const QColor kColor1;
    static const QColor kColor2;
    static const QColor kColor3;
};
