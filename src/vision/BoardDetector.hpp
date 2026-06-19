#pragma once
#include "BoardCalibration.hpp"
#include "ScreenCapture.hpp"
#include "TemplatePieceRecognizer.hpp"
#include "FrameStabilityDetector.hpp"
#include "../chess/Fen.hpp"
#include <QImage>
#include <array>
#include <string>

class BoardDetector {
public:
    explicit BoardDetector(const CalibrationData& cal);

    void setTemplateDir(const std::string& dir);
    void setCalibration(const CalibrationData& cal);

    std::string detectFen(const QImage& screenShot, bool whiteToMove);

    QImage extractBoard(const QImage& screenShot) const;

    bool hasTemplates() const;

private:
    CalibrationData         m_cal;
    TemplatePieceRecognizer m_recognizer;
    bool                    m_templatesLoaded = false;
};
