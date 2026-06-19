#pragma once
#include <QImage>
#include <QRect>
#include <memory>

class IDesktopCapture {
public:
    virtual ~IDesktopCapture() = default;
    virtual QImage captureScreen(int screenIndex = 0) = 0;
    virtual QImage captureRect(const QRect& rect, int screenIndex = 0) = 0;

    static std::unique_ptr<IDesktopCapture> create();
};
