#pragma once
#include <chess.hpp>
#include <string>
#include <optional>

class Board {
public:
    Board();
    explicit Board(const std::string& fen);

    void reset();
    bool setFen(const std::string& fen);
    std::string fen() const;

    bool applyMove(const std::string& uciMove);
    bool isLegalMove(const std::string& uciMove) const;

    chess::Color sideToMove() const;
    bool isGameOver() const;

    const chess::Board& inner() const { return m_board; }

private:
    chess::Board m_board;
};
