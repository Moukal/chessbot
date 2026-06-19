#pragma once
#include "Board.hpp"
#include "Fen.hpp"
#include <string>
#include <optional>

class PositionValidator {
public:
    explicit PositionValidator();

    void setCurrentFen(const std::string& fen);
    std::string currentFen() const;

    std::optional<std::string> validateTransition(const std::string& newFen) const;
    bool isConsistent(const std::string& fen) const;
    void reset();

private:
    Board m_board;
};
