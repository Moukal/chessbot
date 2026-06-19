#pragma once
#include "UciParser.hpp"
#include <functional>
#include <string>
#include <vector>

using InfoCallback     = std::function<void(const UciInfo&)>;
using BestMoveCallback = std::function<void(const UciBestMove&)>;

class IUciEngine {
public:
    virtual ~IUciEngine() = default;

    virtual bool start() = 0;
    virtual void stop()  = 0;
    virtual bool isRunning() const = 0;

    virtual void setPosition(const std::string& fen) = 0;
    virtual void go(int moveTimeMs, int multiPV = 3)  = 0;
    virtual void stopSearch() = 0;

    virtual void setOption(const std::string& name, const std::string& value) = 0;

    virtual void onInfo(InfoCallback cb)         = 0;
    virtual void onBestMove(BestMoveCallback cb) = 0;
};
