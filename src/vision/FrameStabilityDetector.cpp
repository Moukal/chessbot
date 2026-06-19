#include "FrameStabilityDetector.hpp"
#include <QColor>

FrameStabilityDetector::FrameStabilityDetector(int requiredStableFrames, double diffThreshold)
    : m_required(requiredStableFrames), m_threshold(diffThreshold)
{}

bool FrameStabilityDetector::feed(const QImage& frame) {
    if (m_prev.isNull()) {
        m_prev = frame.copy();
        m_stableCount = 0;
        return false;
    }

    m_lastDiff = pixelDiff(frame, m_prev);
    m_prev = frame.copy();

    if (m_lastDiff <= m_threshold) {
        ++m_stableCount;
    } else {
        m_stableCount = 0;
    }

    return isStable();
}

void FrameStabilityDetector::reset() {
    m_prev = {};
    m_stableCount = 0;
    m_lastDiff = 0.0;
}

double FrameStabilityDetector::pixelDiff(const QImage& a, const QImage& b) {
    if (a.size() != b.size()) return 1.0;

    const QImage ia = a.convertToFormat(QImage::Format_RGB32);
    const QImage ib = b.convertToFormat(QImage::Format_RGB32);

    long long total = 0;
    const int w = ia.width(), h = ia.height();

    for (int y = 0; y < h; y += 2) {
        const QRgb* rowA = reinterpret_cast<const QRgb*>(ia.constScanLine(y));
        const QRgb* rowB = reinterpret_cast<const QRgb*>(ib.constScanLine(y));
        for (int x = 0; x < w; x += 2) {
            int dr = qRed(rowA[x])   - qRed(rowB[x]);
            int dg = qGreen(rowA[x]) - qGreen(rowB[x]);
            int db = qBlue(rowA[x])  - qBlue(rowB[x]);
            total += dr*dr + dg*dg + db*db;
        }
    }

    double pixels = (double)(w/2) * (double)(h/2);
    return (pixels > 0) ? (double)total / (pixels * 3.0 * 255.0 * 255.0) : 0.0;
}
