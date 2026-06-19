#include "Board.hpp"
#include "../infrastructure/Logger.hpp"

Board::Board() {
    m_board.setFen(chess::constants::STARTPOS);
}

Board::Board(const std::string& fen) {
    setFen(fen);
}

void Board::reset() {
    m_board.setFen(chess::constants::STARTPOS);
}

bool Board::setFen(const std::string& fen) {
    try {
        m_board.setFen(fen);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Invalid FEN '{}': {}", fen, e.what());
        return false;
    }
}

std::string Board::fen() const {
    return m_board.getFen();
}

bool Board::applyMove(const std::string& uciMove) {
    try {
        chess::Move move = chess::uci::uciToMove(m_board, uciMove);
        if (move == chess::Move::NULL_MOVE) {
            LOG_WARN("Null move from UCI string: {}", uciMove);
            return false;
        }
        m_board.makeMove(move);
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to apply move '{}': {}", uciMove, e.what());
        return false;
    }
}

bool Board::isLegalMove(const std::string& uciMove) const {
    try {
        chess::Movelist moves;
        chess::movegen::legalmoves(moves, m_board);
        chess::Move mv = chess::uci::uciToMove(m_board, uciMove);
        for (const auto& m : moves) {
            if (m == mv) return true;
        }
        return false;
    } catch (...) {
        return false;
    }
}

chess::Color Board::sideToMove() const {
    return m_board.sideToMove();
}

bool Board::isGameOver() const {
    auto [reason, result] = m_board.isGameOver();
    return reason != chess::GameResultReason::NONE;
}
