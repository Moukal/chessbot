#pragma once
#include <QImage>

class FrameStabilityDetector {
public:
    explicit FrameStabilityDetector(int requiredStableFrames = 3, double diffThreshold = 0.02);

    bool feed(const QImage& frame);
    bool isStable() const { return m_stableCount >= m_required; }
    void reset();

    double lastDiff() const { return m_lastDiff; }
    int stableCount() const { return m_stableCount; }
    int requiredStableFrames() const { return m_required; }
    double threshold() const { return m_threshold; }

private:
    int    m_required;
    double m_threshold;
    int    m_stableCount = 0;
    QImage m_prev;
    double m_lastDiff = 0.0;

    static double pixelDiff(const QImage& a, const QImage& b);
};
