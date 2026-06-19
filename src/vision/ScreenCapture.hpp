#pragma once
#include "../platform/IDesktopCapture.hpp"
#include <QImage>
#include <QRect>
#include <memory>

class ScreenCapture {
public:
    ScreenCapture();

    QImage grabFullScreen(int screenIndex = 0);
    QImage grabRect(const QRect& rect, int screenIndex = 0);

private:
    std::unique_ptr<IDesktopCapture> m_capture;
};
