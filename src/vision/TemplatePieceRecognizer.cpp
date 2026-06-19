#include "TemplatePieceRecognizer.hpp"
#include "../infrastructure/Logger.hpp"
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <filesystem>
#include <vector>
#include <QImage>

static const std::map<std::string, Piece> kFilenameToPiece = {
    {"wP", Piece::WP}, {"wN", Piece::WN}, {"wB", Piece::WB},
    {"wR", Piece::WR}, {"wQ", Piece::WQ}, {"wK", Piece::WK},
    {"bP", Piece::BP}, {"bN", Piece::BN}, {"bB", Piece::BB},
    {"bR", Piece::BR}, {"bQ", Piece::BQ}, {"bK", Piece::BK},
};

bool TemplatePieceRecognizer::loadTemplates(const std::string& templateDir) {
    m_templates.clear();
    namespace fs = std::filesystem;

    if (!fs::exists(templateDir)) {
        LOG_WARN("Template directory not found: {}", templateDir);
        return false;
    }

    int loaded = 0;
    for (const auto& entry : fs::directory_iterator(templateDir)) {
        std::string stem = entry.path().stem().string();
        auto it = kFilenameToPiece.find(stem);
        if (it == kFilenameToPiece.end()) continue;

        cv::Mat tmpl = cv::imread(entry.path().string(), cv::IMREAD_UNCHANGED);
        if (tmpl.empty()) {
            LOG_WARN("Failed to load template: {}", entry.path().string());
            continue;
        }
        cv::resize(tmpl, tmpl, cv::Size(64, 64));

        PieceTemplate pieceTemplate;
        if (tmpl.channels() == 4) {
            std::vector<cv::Mat> channels;
            cv::split(tmpl, channels);
            cv::merge(std::vector<cv::Mat>{channels[0], channels[1], channels[2]}, tmpl);
            cv::threshold(channels[3], pieceTemplate.mask, 8, 255, cv::THRESH_BINARY);
        } else {
            pieceTemplate.mask = cv::Mat(64, 64, CV_8UC1, cv::Scalar(255));
        }

        cv::cvtColor(tmpl, pieceTemplate.gray, cv::COLOR_BGR2GRAY);
        m_templates[it->second] = pieceTemplate;
        ++loaded;
    }

    LOG_INFO("Loaded {} piece templates from {}", loaded, templateDir);
    return loaded > 0;
}

cv::Mat TemplatePieceRecognizer::qImageToMat(const QImage& img) const {
    QImage rgb = img.convertToFormat(QImage::Format_RGB888);
    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                const_cast<uchar*>(rgb.constBits()),
                static_cast<size_t>(rgb.bytesPerLine()));
    cv::Mat result;
    cv::cvtColor(mat.clone(), result, cv::COLOR_RGB2GRAY);
    return result;
}

Piece TemplatePieceRecognizer::matchSquare(const cv::Mat& squareGray) const {
    cv::Mat sq;
    cv::resize(squareGray, sq, cv::Size(64, 64));

    Piece best = Piece::None;
    double bestScore = m_threshold;

    for (const auto& [piece, tmpl] : m_templates) {
        cv::Mat result;
        cv::matchTemplate(sq, tmpl.gray, result, cv::TM_CCORR_NORMED, tmpl.mask);
        double maxVal;
        cv::minMaxLoc(result, nullptr, &maxVal);
        if (maxVal > bestScore) {
            bestScore = maxVal;
            best = piece;
        }
    }

    return best;
}

std::array<Piece, 64> TemplatePieceRecognizer::recognize(const QImage& boardImage) {
    std::array<Piece, 64> result{};
    result.fill(Piece::None);

    if (boardImage.isNull()) {
        LOG_ERROR("Piece recognition skipped: board image is null");
        return result;
    }

    cv::Mat boardMat = qImageToMat(boardImage);

    int sw = boardMat.cols / 8;
    int sh = boardMat.rows / 8;
    if (sw <= 0 || sh <= 0) {
        LOG_ERROR("Piece recognition skipped: board image too small ({}x{})",
                  boardImage.width(), boardImage.height());
        return result;
    }

    int recognized = 0;
    int whitePieces = 0;
    int blackPieces = 0;
    int whiteKings = 0;
    int blackKings = 0;
    for (int rank = 0; rank < 8; ++rank) {
        for (int file = 0; file < 8; ++file) {
            cv::Rect roi(file * sw, (7 - rank) * sh, sw, sh);
            if (roi.x + roi.width  > boardMat.cols ||
                roi.y + roi.height > boardMat.rows)
                continue;

            cv::Mat square = boardMat(roi);
            Piece piece = matchSquare(square);
            if (piece != Piece::None) {
                ++recognized;
                if (piece >= Piece::WP && piece <= Piece::WK)
                    ++whitePieces;
                else if (piece >= Piece::BP && piece <= Piece::BK)
                    ++blackPieces;
                if (piece == Piece::WK)
                    ++whiteKings;
                else if (piece == Piece::BK)
                    ++blackKings;
            }
            result[rank * 8 + file] = piece;
        }
    }

    if (recognized == 0) {
        LOG_WARN("No pieces recognized from board image ({}x{}, templates={}, threshold={})",
                 boardImage.width(), boardImage.height(), m_templates.size(), m_threshold);
    } else {
        LOG_DEBUG("Recognized {} pieces from board image (white={} black={} wK={} bK={} board={}x{} square={}x{} threshold={})",
                  recognized, whitePieces, blackPieces, whiteKings, blackKings,
                  boardImage.width(), boardImage.height(), sw, sh, m_threshold);
    }

    return result;
}
