#include "BoardCalibration.hpp"
#include "../infrastructure/Logger.hpp"
#include <nlohmann/json.hpp>

QRect CalibrationData::squareRect(int file, int rank) const {
    int sw = boardRect.width()  / 8;
    int sh = boardRect.height() / 8;

    int displayFile = playAsWhite ? file : (7 - file);
    int displayRank = playAsWhite ? (7 - rank) : rank;

    return QRect(
        boardRect.x() + displayFile * sw,
        boardRect.y() + displayRank * sh,
        sw, sh
    );
}

QPoint CalibrationData::squareCenter(int file, int rank) const {
    QRect r = squareRect(file, rank);
    return r.center();
}

QRect CalibrationData::squareRectScreen(int file, int rank) const {
    return squareRect(file, rank);
}

BoardCalibration::BoardCalibration() {}

void BoardCalibration::setCornerA8(QPoint p) { m_cornerA8 = p; }
void BoardCalibration::setCornerH1(QPoint p)  { m_cornerH1 = p; }

bool BoardCalibration::finalize(bool playAsWhite) {
    if (m_cornerA8.isNull() || m_cornerH1.isNull()) {
        LOG_ERROR("Both corners must be set before finalizing calibration");
        return false;
    }
    int x = std::min(m_cornerA8.x(), m_cornerH1.x());
    int y = std::min(m_cornerA8.y(), m_cornerH1.y());
    int w = std::abs(m_cornerH1.x() - m_cornerA8.x());
    int h = std::abs(m_cornerH1.y() - m_cornerA8.y());

    if (w < 100 || h < 100) {
        LOG_ERROR("Board rect too small: {}x{}", w, h);
        return false;
    }

    m_data.boardRect    = QRect(x, y, w, h);
    m_data.playAsWhite  = playAsWhite;
    m_data.calibrated   = true;
    LOG_INFO("Calibration finalized: rect={},{},{},{} side={}",
             x, y, w, h, playAsWhite ? "white" : "black");
    return true;
}

bool BoardCalibration::loadFromJson(const std::string& json) {
    try {
        auto j = nlohmann::json::parse(json);
        m_data.boardRect = QRect(
            j["x"].get<int>(), j["y"].get<int>(),
            j["w"].get<int>(), j["h"].get<int>()
        );
        m_data.playAsWhite = j.value("playAsWhite", true);
        m_data.calibrated  = true;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load calibration: {}", e.what());
        return false;
    }
}

std::string BoardCalibration::toJson() const {
    nlohmann::json j = {
        {"x", m_data.boardRect.x()},
        {"y", m_data.boardRect.y()},
        {"w", m_data.boardRect.width()},
        {"h", m_data.boardRect.height()},
        {"playAsWhite", m_data.playAsWhite}
    };
    return j.dump();
}
