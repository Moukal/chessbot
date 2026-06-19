#pragma once
#include <QRect>
#include <QPoint>
#include <QImage>
#include <array>
#include <string>

struct CalibrationData {
    QRect  boardRect;
    bool   playAsWhite = true;
    bool   calibrated  = false;

    QRect  squareRect(int file, int rank) const;
    QPoint squareCenter(int file, int rank) const;

    // file/rank in 0..7
    QRect  squareRectScreen(int file, int rank) const;
};

class BoardCalibration {
public:
    BoardCalibration();

    bool loadFromJson(const std::string& json);
    std::string toJson() const;

    CalibrationData data() const { return m_data; }
    void setData(const CalibrationData& d) { m_data = d; }

    bool isCalibrated() const { return m_data.calibrated; }

    // Manual two-click calibration: click A8 corner then H1 corner
    void setCornerA8(QPoint p);
    void setCornerH1(QPoint p);
    bool finalize(bool playAsWhite = true);

private:
    CalibrationData m_data;
    QPoint m_cornerA8;
    QPoint m_cornerH1;
};
