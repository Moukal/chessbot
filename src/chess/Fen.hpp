#pragma once
#include <string>
#include <array>

enum class Piece { None=0, WP,WN,WB,WR,WQ,WK, BP,BN,BB,BR,BQ,BK };

struct FenBoard {
    std::array<Piece, 64> squares{};
    bool whiteToMove = true;
    std::string castling;
    std::string enPassant;
    int halfMove = 0;
    int fullMove = 1;

    static constexpr int sq(int file, int rank) { return rank * 8 + file; }
    Piece at(int file, int rank) const { return squares[sq(file, rank)]; }
};

class Fen {
public:
    static std::string build(const FenBoard& fb);
    static FenBoard    parse(const std::string& fen);
    static bool        isValid(const std::string& fen);
};
