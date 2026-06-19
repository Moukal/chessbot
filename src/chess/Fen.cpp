#include "Fen.hpp"
#include <sstream>
#include <stdexcept>
#include <map>

static const std::map<char, Piece> kCharToPiece = {
    {'P', Piece::WP}, {'N', Piece::WN}, {'B', Piece::WB},
    {'R', Piece::WR}, {'Q', Piece::WQ}, {'K', Piece::WK},
    {'p', Piece::BP}, {'n', Piece::BN}, {'b', Piece::BB},
    {'r', Piece::BR}, {'q', Piece::BQ}, {'k', Piece::BK},
};

static const std::map<Piece, char> kPieceToChar = {
    {Piece::WP,'P'},{Piece::WN,'N'},{Piece::WB,'B'},
    {Piece::WR,'R'},{Piece::WQ,'Q'},{Piece::WK,'K'},
    {Piece::BP,'p'},{Piece::BN,'n'},{Piece::BB,'b'},
    {Piece::BR,'r'},{Piece::BQ,'q'},{Piece::BK,'k'},
};

FenBoard Fen::parse(const std::string& fen) {
    FenBoard fb;
    std::istringstream ss(fen);
    std::string piecePart, side, castling, ep;
    int half = 0, full = 1;
    ss >> piecePart >> side >> castling >> ep >> half >> full;

    int rank = 7, file = 0;
    for (char c : piecePart) {
        if (c == '/') {
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += (c - '0');
        } else {
            auto it = kCharToPiece.find(c);
            if (it != kCharToPiece.end())
                fb.squares[FenBoard::sq(file, rank)] = it->second;
            ++file;
        }
    }
    fb.whiteToMove = (side == "w");
    fb.castling    = castling;
    fb.enPassant   = ep;
    fb.halfMove    = half;
    fb.fullMove    = full;
    return fb;
}

std::string Fen::build(const FenBoard& fb) {
    std::string result;
    for (int r = 7; r >= 0; --r) {
        int empty = 0;
        for (int f = 0; f < 8; ++f) {
            Piece p = fb.at(f, r);
            if (p == Piece::None) {
                ++empty;
            } else {
                if (empty) { result += ('0' + empty); empty = 0; }
                result += kPieceToChar.at(p);
            }
        }
        if (empty) result += ('0' + empty);
        if (r > 0) result += '/';
    }
    result += ' ';
    result += fb.whiteToMove ? 'w' : 'b';
    result += ' ';
    result += fb.castling.empty() ? "-" : fb.castling;
    result += ' ';
    result += fb.enPassant.empty() ? "-" : fb.enPassant;
    result += ' ';
    result += std::to_string(fb.halfMove);
    result += ' ';
    result += std::to_string(fb.fullMove);
    return result;
}

bool Fen::isValid(const std::string& fen) {
    if (fen.empty()) return false;
    try {
        FenBoard fb = parse(fen);
        // Must have exactly one white and one black king
        int wk = 0, bk = 0;
        for (auto p : fb.squares) {
            if (p == Piece::WK) ++wk;
            if (p == Piece::BK) ++bk;
        }
        return wk == 1 && bk == 1;
    } catch (...) {
        return false;
    }
}
