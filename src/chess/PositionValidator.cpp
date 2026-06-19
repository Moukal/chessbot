#include "PositionValidator.hpp"
#include "../infrastructure/Logger.hpp"

PositionValidator::PositionValidator() {}

void PositionValidator::setCurrentFen(const std::string& fen) {
    m_board.setFen(fen);
}

std::string PositionValidator::currentFen() const {
    return m_board.fen();
}

std::optional<std::string> PositionValidator::validateTransition(const std::string& newFen) const {
    // Try all legal moves from current position and see if any leads to newFen
    chess::Board tmp = m_board.inner();
    chess::Movelist moves;
    chess::movegen::legalmoves(moves, tmp);

    for (const auto& move : moves) {
        chess::Board candidate = tmp;
        candidate.makeMove(move);
        if (candidate.getFen() == newFen) {
            return chess::uci::moveToUci(move);
        }
    }

    LOG_WARN("No legal move found transitioning to FEN: {}", newFen);
    return std::nullopt;
}

bool PositionValidator::isConsistent(const std::string& fen) const {
    return Fen::isValid(fen);
}

void PositionValidator::reset() {
    m_board.reset();
}
