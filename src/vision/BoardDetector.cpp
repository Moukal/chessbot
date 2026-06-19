#include "BoardDetector.hpp"
#include "../infrastructure/Logger.hpp"

BoardDetector::BoardDetector(const CalibrationData& cal)
    : m_cal(cal)
{}

void BoardDetector::setTemplateDir(const std::string& dir) {
    m_templatesLoaded = m_recognizer.loadTemplates(dir);
    LOG_INFO("BoardDetector template state: dir='{}' loaded={}", dir, m_templatesLoaded ? "true" : "false");
}

void BoardDetector::setCalibration(const CalibrationData& cal) {
    m_cal = cal;
    LOG_INFO("BoardDetector calibration updated: calibrated={} rect=({},{} {}x{}) playAsWhite={}",
             m_cal.calibrated ? "true" : "false",
             m_cal.boardRect.x(), m_cal.boardRect.y(), m_cal.boardRect.width(), m_cal.boardRect.height(),
             m_cal.playAsWhite ? "true" : "false");
}

bool BoardDetector::hasTemplates() const {
    return m_templatesLoaded;
}

QImage BoardDetector::extractBoard(const QImage& screenShot) const {
    if (screenShot.isNull() || !m_cal.calibrated) return {};
    if (!screenShot.rect().contains(m_cal.boardRect)) {
        LOG_ERROR("Cannot extract board: rect=({},{} {}x{}) outside screenshot={}x{}",
                  m_cal.boardRect.x(), m_cal.boardRect.y(), m_cal.boardRect.width(), m_cal.boardRect.height(),
                  screenShot.width(), screenShot.height());
        return {};
    }
    return screenShot.copy(m_cal.boardRect);
}

std::string BoardDetector::detectFen(const QImage& screenShot, bool whiteToMove) {
    if (!m_cal.calibrated) {
        LOG_ERROR("Board not calibrated");
        return {};
    }

    QImage boardImg = extractBoard(screenShot);
    if (boardImg.isNull()) {
        LOG_ERROR("Failed to extract board image");
        return {};
    }
    LOG_TRACE("Board image extracted: {}x{}", boardImg.width(), boardImg.height());

    std::array<Piece, 64> pieces = m_recognizer.recognize(boardImg);

    FenBoard fb;
    fb.squares    = pieces;
    fb.whiteToMove = whiteToMove;
    fb.castling   = "-";
    fb.enPassant  = "-";
    fb.halfMove   = 0;
    fb.fullMove   = 1;

    if (!m_cal.playAsWhite) {
        // Flip board: rank 0 is at top visually for black
        std::array<Piece, 64> flipped{};
        for (int r = 0; r < 8; ++r)
            for (int f = 0; f < 8; ++f)
                flipped[r*8+f] = pieces[(7-r)*8+(7-f)];
        fb.squares = flipped;
    }

    std::string fen = Fen::build(fb);
    int wK = 0, bK = 0, total = 0;
    for (Piece p : fb.squares) {
        if (p == Piece::None)
            continue;
        ++total;
        if (p == Piece::WK)
            ++wK;
        else if (p == Piece::BK)
            ++bK;
    }
    LOG_DEBUG("BoardDetector generated FEN (pieces={} wK={} bK={}): {}", total, wK, bK, fen);
    return fen;
}
