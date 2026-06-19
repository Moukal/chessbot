#include "IDesktopCapture.hpp"
#include "../infrastructure/Logger.hpp"
#include <QApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QScreen>
#include <QPixmap>
#include <QImage>
#include <QTemporaryFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QUrl>
#include <QTimer>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

class PortalResponseWaiter : public QObject {
    Q_OBJECT
public:
    explicit PortalResponseWaiter(QString requestPath)
        : m_requestPath(std::move(requestPath))
    {
        QDBusConnection::sessionBus().connect(
            "org.freedesktop.portal.Desktop",
            m_requestPath,
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onResponse(uint,QVariantMap))
        );
    }

    ~PortalResponseWaiter() override {
        QDBusConnection::sessionBus().disconnect(
            "org.freedesktop.portal.Desktop",
            m_requestPath,
            "org.freedesktop.portal.Request",
            "Response",
            this,
            SLOT(onResponse(uint,QVariantMap))
        );
    }

    bool wait(int timeoutMs = 60000) {
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, &m_loop, &QEventLoop::quit);
        timer.start(timeoutMs);
        m_loop.exec();
        return m_received && m_responseCode == 0;
    }

    uint responseCode() const { return m_responseCode; }
    const QVariantMap& results() const { return m_results; }

public slots:
    void onResponse(uint responseCode, const QVariantMap& results) {
        m_responseCode = responseCode;
        m_results = results;
        m_received = true;
        m_loop.quit();
    }

private:
    QString m_requestPath;
    QEventLoop m_loop;
    QVariantMap m_results;
    uint m_responseCode = 2;
    bool m_received = false;
};


class QtDesktopCapture : public IDesktopCapture {
public:
    QImage captureScreen(int screenIndex = 0) override {
        // 1. grim: Wayland native, works on GNOME 41+ (install: sudo apt install grim)
        const char* wayland = std::getenv("WAYLAND_DISPLAY");
        LOG_TRACE("Capture request: screenIndex={} XDG_SESSION_TYPE='{}' XDG_CURRENT_DESKTOP='{}' WAYLAND_DISPLAY='{}' DISPLAY='{}'",
                  screenIndex,
                  std::getenv("XDG_SESSION_TYPE") ? std::getenv("XDG_SESSION_TYPE") : "",
                  std::getenv("XDG_CURRENT_DESKTOP") ? std::getenv("XDG_CURRENT_DESKTOP") : "",
                  wayland ? wayland : "",
                  std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "");
        // 1. grim — Wayland native, fast (~30ms), works on GNOME 41+
        if (wayland && *wayland) {
            QImage img = captureGrim(wayland);
            if (!img.isNull()) return img;
        }

        // 2. XDG Screenshot portal — fallback if grim absent
        if (wayland && *wayland) {
            QImage portalImg = captureWithScreenshotPortal();
            if (!portalImg.isNull() && isFullRes(portalImg))
                return portalImg;
        }

        // 3. Qt grabWindow (pure X11 only, black on XWayland)
        if (QImage qtImage = captureWithQt(screenIndex); !isBlack(qtImage))
            return qtImage;

        // 4. XGetImage (pure X11 only)
        if (QImage x11Image = captureWithX11(); !x11Image.isNull())
            return x11Image;

        LOG_ERROR("All capture backends failed");
        return {};
    }

    QImage captureRect(const QRect& rect, int screenIndex = 0) override {
        QImage full = captureScreen(screenIndex);
        if (full.isNull()) return {};
        return full.copy(rect);
    }

private:
    // XDG Desktop Portal Screenshot — single frame, works on GNOME Wayland
    static QImage captureWithScreenshotPortal() {
        QDBusInterface portal(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.Screenshot",
            QDBusConnection::sessionBus()
        );
        if (!portal.isValid()) return {};

        QVariantMap options;
        options["interactive"] = false;
        options["modal"]       = false;

        QDBusMessage reply = portal.call("Screenshot", QString(), options);
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty())
            return {};

        QString requestPath = reply.arguments().at(0).value<QDBusObjectPath>().path();
        PortalResponseWaiter waiter(requestPath);
        if (!waiter.wait(3000)) return {};

        QString uri = waiter.results().value("uri").toString();
        if (uri.isEmpty()) return {};

        // Convert file:// URI to local path
        QString path = QUrl(uri).toLocalFile();
        QImage img(path);
        QFile::remove(path);

        if (!img.isNull())
            LOG_TRACE("Captured screen with XDG Screenshot portal: {}x{}",
                      img.width(), img.height());
        return img;
    }

    static QImage captureGrim(const char* waylandDisplay) {
        QTemporaryFile temp(QDir::tempPath() + "/chessbotx-grim-XXXXXX.png");
        temp.setAutoRemove(false);
        if (!temp.open())
            return {};
        const QString path = temp.fileName();
        temp.close();
        QFile::remove(path);

        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("WAYLAND_DISPLAY", waylandDisplay);
        proc.setProcessEnvironment(env);
        proc.start("grim", {"-t", "png", path});
        if (!proc.waitForFinished(3000) || proc.exitCode() != 0) {
            LOG_WARN("grim failed: exitCode={} stderr='{}'",
                     proc.exitCode(),
                     QString::fromUtf8(proc.readAllStandardError()).trimmed().toStdString());
            QFile::remove(path);
            return {};
        }
        QImage img(path);
        QFile::remove(path);
        if (!img.isNull())
            LOG_TRACE("Captured screen with grim: {}x{}", img.width(), img.height());
        return img;
    }

    // Reject captures smaller than HD — portal sometimes returns 640x480
    // before the stream negotiates native resolution.
    static bool isFullRes(const QImage& img) {
        return img.width() >= 1280 && img.height() >= 720;
    }

    static bool isBlack(const QImage& img) {
        if (img.isNull() || img.width() < 4 || img.height() < 4) return true;
        int checks = 0;
        for (int y : {img.height()/4, img.height()/2, 3*img.height()/4}) {
            for (int x : {img.width()/4, img.width()/2, 3*img.width()/4}) {
                QRgb px = img.pixel(x, y);
                if (qRed(px) > 10 || qGreen(px) > 10 || qBlue(px) > 10) ++checks;
            }
        }
        return checks == 0;
    }

    QImage captureWithQt(int screenIndex) {
        QList<QScreen*> screens = QApplication::screens();
        if (screenIndex < 0 || screenIndex >= screens.size()) {
            LOG_ERROR("Screen index {} out of range (have {})", screenIndex, screens.size());
            return {};
        }

        QPixmap pixmap = screens[screenIndex]->grabWindow(0);
        if (pixmap.isNull()) {
            LOG_WARN("QScreen::grabWindow returned a null pixmap");
            return {};
        }

        QImage image = pixmap.toImage();
        if (image.isNull()) {
            LOG_WARN("QScreen::grabWindow produced a null image");
            return {};
        }

        LOG_TRACE("Captured screen with Qt backend: {}x{}", image.width(), image.height());
        return image;
    }

    QImage captureWithX11() {
#ifdef Q_OS_LINUX
        const char* sessionType = std::getenv("XDG_SESSION_TYPE");
        if (sessionType && std::strcmp(sessionType, "wayland") == 0) {
            LOG_WARN("Skipping X11 root capture under Wayland session");
            return {};
        }

        Display* dpy = XOpenDisplay(nullptr);
        if (!dpy) {
            LOG_ERROR("XOpenDisplay failed");
            return {};
        }

        Window root = DefaultRootWindow(dpy);
        XWindowAttributes gwa;
        XGetWindowAttributes(dpy, root, &gwa);

        XImage* ximg = XGetImage(dpy, root, 0, 0,
                                 gwa.width, gwa.height,
                                 AllPlanes, ZPixmap);

        if (!ximg) {
            LOG_ERROR("XGetImage failed");
            XCloseDisplay(dpy);
            return {};
        }

        auto maskShift = [](unsigned long mask) {
            int shift = 0;
            while (mask && (mask & 1UL) == 0) {
                mask >>= 1;
                ++shift;
            }
            return shift;
        };
        const int rShift = maskShift(ximg->red_mask);
        const int gShift = maskShift(ximg->green_mask);
        const int bShift = maskShift(ximg->blue_mask);

        // Convert XImage to QImage before releasing the X11 resources.
        QImage result(ximg->width, ximg->height, QImage::Format_RGB32);
        for (int y = 0; y < ximg->height; ++y) {
            QRgb* dst = reinterpret_cast<QRgb*>(result.scanLine(y));
            for (int x = 0; x < ximg->width; ++x) {
                unsigned long px = XGetPixel(ximg, x, y);
                int r = static_cast<int>((px & ximg->red_mask) >> rShift);
                int g = static_cast<int>((px & ximg->green_mask) >> gShift);
                int b = static_cast<int>((px & ximg->blue_mask) >> bShift);
                dst[x] = qRgb(r, g, b);
            }
        }

        XDestroyImage(ximg);
        XCloseDisplay(dpy);
        LOG_TRACE("Captured screen with X11 backend: {}x{}", result.width(), result.height());
        return result;
#else
        return {};
#endif
    }
};

std::unique_ptr<IDesktopCapture> IDesktopCapture::create() {
    return std::make_unique<QtDesktopCapture>();
}

#include "DesktopCapture.moc"
