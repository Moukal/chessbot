#include <catch2/catch_test_macros.hpp>
#include "chess/Fen.hpp"

static const std::string kStartFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

TEST_CASE("Fen - parse start position", "[fen]") {
    auto fb = Fen::parse(kStartFen);
    CHECK(fb.whiteToMove == true);
    CHECK(fb.castling    == "KQkq");
    CHECK(fb.enPassant   == "-");
    CHECK(fb.halfMove    == 0);
    CHECK(fb.fullMove    == 1);

    // White pawns on rank 1 (index 1)
    for (int f = 0; f < 8; ++f)
        CHECK(fb.at(f, 1) == Piece::WP);

    // Black pawns on rank 6
    for (int f = 0; f < 8; ++f)
        CHECK(fb.at(f, 6) == Piece::BP);

    // White pieces on rank 0
    CHECK(fb.at(0, 0) == Piece::WR);
    CHECK(fb.at(1, 0) == Piece::WN);
    CHECK(fb.at(4, 0) == Piece::WK);

    // Black pieces on rank 7
    CHECK(fb.at(4, 7) == Piece::BK);
    CHECK(fb.at(3, 7) == Piece::BQ);
}

TEST_CASE("Fen - build round-trips", "[fen]") {
    FenBoard fb = Fen::parse(kStartFen);
    std::string rebuilt = Fen::build(fb);
    CHECK(rebuilt == kStartFen);
}

TEST_CASE("Fen - isValid start position", "[fen]") {
    CHECK(Fen::isValid(kStartFen));
}

TEST_CASE("Fen - isValid rejects no kings", "[fen]") {
    CHECK(!Fen::isValid("8/8/8/8/8/8/8/8 w - - 0 1"));
}

TEST_CASE("Fen - isValid rejects empty string", "[fen]") {
    CHECK(!Fen::isValid(""));
}

TEST_CASE("Fen - parse after 1.e4", "[fen]") {
    std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
    auto fb = Fen::parse(fen);
    CHECK(fb.whiteToMove == false);
    CHECK(fb.enPassant   == "e3");
    CHECK(fb.at(4, 3)    == Piece::WP);
    CHECK(fb.at(4, 1)    == Piece::None);
}

TEST_CASE("Fen - empty squares", "[fen]") {
    FenBoard fb = Fen::parse(kStartFen);
    // Ranks 2-5 are empty
    for (int r = 2; r <= 5; ++r)
        for (int f = 0; f < 8; ++f)
            CHECK(fb.at(f, r) == Piece::None);
}
