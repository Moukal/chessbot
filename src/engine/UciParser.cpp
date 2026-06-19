#include "UciParser.hpp"
#include <sstream>
#include <map>

UciMove UciParser::parseUciMove(const std::string& s) {
    if (s.size() < 4) return {};
    UciMove m;
    m.from = s.substr(0, 2);
    m.to   = s.substr(2, 2);
    if (s.size() == 5)
        m.promotion = s.substr(4, 1);
    return m;
}

std::optional<UciInfo> UciParser::parseInfo(const std::string& line) {
    if (line.rfind("info ", 0) != 0)
        return std::nullopt;

    std::istringstream ss(line);
    std::string token;
    ss >> token; // "info"

    UciInfo info;
    bool hasPv = false;

    while (ss >> token) {
        if (token == "depth") {
            ss >> info.depth;
        } else if (token == "multipv") {
            ss >> info.multiPvIndex;
        } else if (token == "score") {
            std::string scoreType;
            ss >> scoreType;
            if (scoreType == "cp") {
                info.scoreMate = false;
                ss >> info.scoreCp;
            } else if (scoreType == "mate") {
                info.scoreMate = true;
                ss >> info.mateIn;
                info.scoreCp = info.mateIn > 0 ? 30000 : -30000;
            }
        } else if (token == "pv") {
            hasPv = true;
            std::string mv;
            while (ss >> mv)
                info.pv.push_back(parseUciMove(mv));
        }
    }

    if (!hasPv || info.pv.empty())
        return std::nullopt;

    return info;
}

std::optional<UciBestMove> UciParser::parseBestMove(const std::string& line) {
    if (line.rfind("bestmove ", 0) != 0)
        return std::nullopt;

    std::istringstream ss(line);
    std::string token;
    ss >> token; // "bestmove"

    UciBestMove bm;
    std::string moveStr;
    ss >> moveStr;

    if (moveStr == "(none)")
        return std::nullopt;

    bm.move = parseUciMove(moveStr);

    if (ss >> token && token == "ponder") {
        std::string ponderStr;
        if (ss >> ponderStr)
            bm.ponder = parseUciMove(ponderStr);
    }

    return bm;
}

bool UciParser::isUciOk(const std::string& line) {
    return line == "uciok";
}

bool UciParser::isReadyOk(const std::string& line) {
    return line == "readyok";
}
