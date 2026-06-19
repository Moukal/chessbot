#pragma once
#include "../chess/Fen.hpp"
#include <QImage>
#include <array>

class IPieceRecognizer {
public:
    virtual ~IPieceRecognizer() = default;
    virtual std::array<Piece, 64> recognize(const QImage& boardImage) = 0;
    virtual bool loadTemplates(const std::string& templateDir) = 0;
};
