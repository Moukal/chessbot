#include "ScreenCapture.hpp"
#include "../infrastructure/Logger.hpp"

ScreenCapture::ScreenCapture()
    : m_capture(IDesktopCapture::create())
{}

QImage ScreenCapture::grabFullScreen(int screenIndex) {
    QImage img = m_capture->captureScreen(screenIndex);
    if (img.isNull())
        LOG_ERROR("Failed to capture screen {}", screenIndex);
    return img;
}

QImage ScreenCapture::grabRect(const QRect& rect, int screenIndex) {
    QImage img = m_capture->captureRect(rect, screenIndex);
    if (img.isNull())
        LOG_ERROR("Failed to capture rect ({},{},{},{}) on screen {}",
                  rect.x(), rect.y(), rect.width(), rect.height(), screenIndex);
    return img;
}
