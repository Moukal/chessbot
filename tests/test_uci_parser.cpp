#include <catch2/catch_test_macros.hpp>
#include "engine/UciParser.hpp"

TEST_CASE("UciParser - parseInfo depth and score", "[uci]") {
    std::string line = "info depth 15 multipv 1 score cp 42 pv e2e4 e7e5 g1f3";
    auto info = UciParser::parseInfo(line);
    REQUIRE(info.has_value());
    CHECK(info->depth       == 15);
    CHECK(info->multiPvIndex == 1);
    CHECK(info->scoreCp     == 42);
    CHECK(!info->scoreMate);
    REQUIRE(info->pv.size() >= 1);
    CHECK(info->pv[0].from == "e2");
    CHECK(info->pv[0].to   == "e4");
}

TEST_CASE("UciParser - parseInfo mate score", "[uci]") {
    std::string line = "info depth 10 multipv 1 score mate 3 pv d1h5";
    auto info = UciParser::parseInfo(line);
    REQUIRE(info.has_value());
    CHECK(info->scoreMate);
    CHECK(info->mateIn == 3);
}

TEST_CASE("UciParser - non-info line returns nullopt", "[uci]") {
    CHECK(!UciParser::parseInfo("bestmove e2e4").has_value());
    CHECK(!UciParser::parseInfo("uciok").has_value());
    CHECK(!UciParser::parseInfo("").has_value());
}

TEST_CASE("UciParser - parseBestMove", "[uci]") {
    auto bm = UciParser::parseBestMove("bestmove e2e4 ponder e7e5");
    REQUIRE(bm.has_value());
    CHECK(bm->move.from == "e2");
    CHECK(bm->move.to   == "e4");
    REQUIRE(bm->ponder.has_value());
    CHECK(bm->ponder->from == "e7");
    CHECK(bm->ponder->to   == "e5");
}

TEST_CASE("UciParser - parseBestMove none", "[uci]") {
    CHECK(!UciParser::parseBestMove("bestmove (none)").has_value());
}

TEST_CASE("UciParser - promotion move", "[uci]") {
    auto bm = UciParser::parseBestMove("bestmove a7a8q");
    REQUIRE(bm.has_value());
    CHECK(bm->move.promotion == "q");
}

TEST_CASE("UciParser - isUciOk / isReadyOk", "[uci]") {
    CHECK(UciParser::isUciOk("uciok"));
    CHECK(!UciParser::isUciOk("readyok"));
    CHECK(UciParser::isReadyOk("readyok"));
    CHECK(!UciParser::isReadyOk("uciok"));
}

TEST_CASE("UciParser - multipv 3 lines", "[uci]") {
    auto i1 = UciParser::parseInfo("info depth 20 multipv 1 score cp 50 pv e2e4");
    auto i2 = UciParser::parseInfo("info depth 20 multipv 2 score cp 30 pv d2d4");
    auto i3 = UciParser::parseInfo("info depth 20 multipv 3 score cp 10 pv g1f3");
    REQUIRE(i1.has_value()); CHECK(i1->multiPvIndex == 1);
    REQUIRE(i2.has_value()); CHECK(i2->multiPvIndex == 2);
    REQUIRE(i3.has_value()); CHECK(i3->multiPvIndex == 3);
}
