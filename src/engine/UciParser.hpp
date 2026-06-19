#pragma once
#include <string>
#include <vector>
#include <optional>

struct UciMove {
    std::string from;
    std::string to;
    std::string promotion;

    std::string uci() const {
        return from + to + promotion;
    }
};

struct UciInfo {
    int depth = 0;
    int multiPvIndex = 1;
    int scoreCp = 0;
    bool scoreMate = false;
    int mateIn = 0;
    std::vector<UciMove> pv;
};

struct UciBestMove {
    UciMove move;
    std::optional<UciMove> ponder;
};

class UciParser {
public:
    static std::optional<UciInfo>     parseInfo(const std::string& line);
    static std::optional<UciBestMove> parseBestMove(const std::string& line);
    static bool                       isUciOk(const std::string& line);
    static bool                       isReadyOk(const std::string& line);

    static UciMove parseUciMove(const std::string& s);
};
