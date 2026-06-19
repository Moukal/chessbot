#pragma once
#include "IPieceRecognizer.hpp"
#include <opencv2/core.hpp>
#include <map>
#include <string>

class TemplatePieceRecognizer : public IPieceRecognizer {
public:
    std::array<Piece, 64> recognize(const QImage& boardImage) override;
    bool loadTemplates(const std::string& templateDir) override;

    double matchThreshold() const      { return m_threshold; }
    void setMatchThreshold(double t)   { m_threshold = t; }

private:
    struct PieceTemplate {
        cv::Mat gray;
        cv::Mat mask;
    };

    cv::Mat qImageToMat(const QImage& img) const;
    Piece matchSquare(const cv::Mat& squareImg) const;

    std::map<Piece, PieceTemplate> m_templates;
    double m_threshold = 0.28; // SQDIFF_NORMED: accept match if score < threshold
};
