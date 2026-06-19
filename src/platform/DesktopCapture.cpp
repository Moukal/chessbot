#include "IDesktopCapture.hpp"
#include "../infrastructure/Logger.hpp"
#include <QApplication>
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QDBusUnixFileDescriptor>
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
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <string>
#include <unistd.h>

#ifdef Q_OS_LINUX
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

struct PortalStream {
    uint nodeId = 0;
    QVariantMap properties;
};

Q_DECLARE_METATYPE(PortalStream)
Q_DECLARE_METATYPE(QList<PortalStream>)

const QDBusArgument& operator>>(const QDBusArgument& argument, PortalStream& stream) {
    argument.beginStructure();
    argument >> stream.nodeId >> stream.properties;
    argument.endStructure();
    return argument;
}

QDBusArgument& operator<<(QDBusArgument& argument, const PortalStream& stream) {
    argument.beginStructure();
    argument << stream.nodeId << stream.properties;
    argument.endStructure();
    return argument;
}

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

struct PortalScreenCastSession {
    QString sessionHandle;
    int pipeWireFd = -1;
    uint nodeId = 0;
    quint64 pipewireSerial = 0;

    std::string targetObject() const {
        if (pipewireSerial > 0)
            return std::to_string(pipewireSerial);
        if (nodeId > 0)
            return std::to_string(nodeId);
        return {};
    }
};

static QString objectPathFromVariant(const QVariant& value) {
    if (value.canConvert<QDBusObjectPath>())
        return value.value<QDBusObjectPath>().path();
    return value.toString();
}

class PortalGStreamerCapture {
public:
    ~PortalGStreamerCapture() {
        stop();
    }

    bool hasFailed() const { return m_failed; }

    QImage captureFrame() {
        if (m_failed) return {};
        if (!ensureStarted()) {
            m_failed = true;   // Don't retry — portal requires user action at startup
            return {};
        }

        GstSample* sample = gst_app_sink_try_pull_sample(m_appSink, 250 * GST_MSECOND);
        if (!sample) {
            LOG_WARN("Portal/GStreamer capture has no frame available yet");
            return {};
        }

        QImage image = sampleToImage(sample);
        gst_sample_unref(sample);
        if (!image.isNull())
            LOG_TRACE("Captured screen with xdg-desktop-portal/PipeWire/GStreamer: {}x{}",
                      image.width(), image.height());
        return image;
    }

private:
    bool ensureStarted() {
        if (m_pipeline && m_appSink)
            return true;

        static std::once_flag gstInitFlag;
        std::call_once(gstInitFlag, []() {
            gst_init(nullptr, nullptr);
        });

        auto session = createPortalSession();
        if (!session) {
            LOG_WARN("Portal ScreenCast session could not be created");
            return false;
        }

        m_pipeWireFd = session->pipeWireFd;
        m_targetObject = session->targetObject();
        LOG_INFO("Portal ScreenCast ready: nodeId={} pipewireSerial={} targetObject='{}' fd={}",
                 session->nodeId, session->pipewireSerial, m_targetObject, m_pipeWireFd);

        return startPipeline();
    }

    std::optional<PortalScreenCastSession> createPortalSession() {
        qDBusRegisterMetaType<PortalStream>();
        qDBusRegisterMetaType<QList<PortalStream>>();

        QDBusInterface portal(
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.ScreenCast",
            QDBusConnection::sessionBus()
        );
        if (!portal.isValid()) {
            LOG_ERROR("ScreenCast portal interface is not valid");
            return std::nullopt;
        }

        const QString tokenBase = QString("chessbotx_%1_%2")
                                      .arg(QCoreApplication::applicationPid())
                                      .arg(reinterpret_cast<quintptr>(this));

        QVariantMap createOptions;
        createOptions["session_handle_token"] = tokenBase + "_session";
        QVariantMap createResults;
        if (!portalRequest(portal.call("CreateSession", createOptions), createResults, "CreateSession"))
            return std::nullopt;

        QString sessionHandle = objectPathFromVariant(createResults.value("session_handle"));
        if (sessionHandle.isEmpty()) {
            LOG_ERROR("ScreenCast CreateSession response did not contain session_handle");
            return std::nullopt;
        }

        QVariantMap selectOptions;
        selectOptions["handle_token"] = tokenBase + "_select";
        selectOptions["types"] = uint(1);      // monitor
        selectOptions["multiple"] = false;
        selectOptions["cursor_mode"] = uint(1);
        if (!portalRequest(portal.call("SelectSources", QDBusObjectPath(sessionHandle), selectOptions),
                           createResults, "SelectSources")) {
            return std::nullopt;
        }

        QVariantMap startOptions;
        startOptions["handle_token"] = tokenBase + "_start";
        QVariantMap startResults;
        if (!portalRequest(portal.call("Start", QDBusObjectPath(sessionHandle), QString(), startOptions),
                           startResults, "Start")) {
            return std::nullopt;
        }

        PortalScreenCastSession session;
        session.sessionHandle = sessionHandle;
        parseStreamInfo(startResults, session);

        QDBusMessage fdReply = portal.call("OpenPipeWireRemote", QDBusObjectPath(sessionHandle), QVariantMap{});
        if (fdReply.type() == QDBusMessage::ErrorMessage) {
            LOG_ERROR("OpenPipeWireRemote failed: {}", fdReply.errorMessage().toStdString());
            return std::nullopt;
        }
        if (fdReply.arguments().isEmpty() ||
            !fdReply.arguments().at(0).canConvert<QDBusUnixFileDescriptor>()) {
            LOG_ERROR("OpenPipeWireRemote did not return a Unix file descriptor");
            return std::nullopt;
        }

        QDBusUnixFileDescriptor dbusFd = fdReply.arguments().at(0).value<QDBusUnixFileDescriptor>();
        session.pipeWireFd = ::dup(dbusFd.fileDescriptor());
        if (session.pipeWireFd < 0) {
            LOG_ERROR("dup() failed for PipeWire remote fd");
            return std::nullopt;
        }
        return session;
    }

    bool portalRequest(const QDBusMessage& reply, QVariantMap& results, const char* name) {
        if (reply.type() == QDBusMessage::ErrorMessage) {
            LOG_ERROR("Portal {} failed immediately: {}", name, reply.errorMessage().toStdString());
            return false;
        }
        if (reply.arguments().isEmpty() ||
            !reply.arguments().at(0).canConvert<QDBusObjectPath>()) {
            LOG_ERROR("Portal {} did not return a request object path", name);
            return false;
        }

        QString requestPath = reply.arguments().at(0).value<QDBusObjectPath>().path();
        LOG_INFO("Portal {} request: {}", name, requestPath.toStdString());
        PortalResponseWaiter waiter(requestPath);
        // Short timeout: if dialog doesn't appear in 5s, skip portal entirely
        const int timeoutMs = (std::string(name) == "Start") ? 5000 : 3000;
        if (!waiter.wait(timeoutMs)) {
            LOG_ERROR("Portal {} response failed/cancelled: code={}", name, waiter.responseCode());
            return false;
        }
        results = waiter.results();
        LOG_INFO("Portal {} response ok", name);
        return true;
    }

    void parseStreamInfo(const QVariantMap& results, PortalScreenCastSession& session) {
        QVariant streamsValue = results.value("streams");
        QList<PortalStream> streams = qdbus_cast<QList<PortalStream>>(streamsValue);
        if (streams.isEmpty()) {
            LOG_WARN("ScreenCast Start response contains no streams");
            return;
        }

        const PortalStream& stream = streams.first();
        session.nodeId = stream.nodeId;
        QVariant serial = stream.properties.value("pipewire-serial");
        if (serial.isValid())
            session.pipewireSerial = serial.toULongLong();

        LOG_INFO("ScreenCast stream selected: nodeId={} pipewireSerial={} properties={}",
                 session.nodeId,
                 session.pipewireSerial,
                 QStringList(stream.properties.keys()).join(",").toStdString());
    }

    bool startPipeline() {
        GstElement* src = gst_element_factory_make("pipewiresrc", "portal-pipewiresrc");
        GstElement* queue = gst_element_factory_make("queue", "portal-queue");
        GstElement* convert = gst_element_factory_make("videoconvert", "portal-videoconvert");
        GstElement* capsFilter = gst_element_factory_make("capsfilter", "portal-caps");
        GstElement* sink = gst_element_factory_make("appsink", "portal-appsink");
        m_pipeline = gst_pipeline_new("portal-capture-pipeline");

        if (!m_pipeline || !src || !queue || !convert || !capsFilter || !sink) {
            LOG_ERROR("Failed to create GStreamer capture pipeline elements");
            return false;
        }

        g_object_set(src, "fd", m_pipeWireFd, nullptr);
        if (!m_targetObject.empty())
            g_object_set(src, "target-object", m_targetObject.c_str(), nullptr);

        g_object_set(queue,
                     "leaky", 2,
                     "max-size-buffers", 1,
                     "max-size-bytes", 0,
                     "max-size-time", 0,
                     nullptr);

        GstCaps* caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", nullptr);
        g_object_set(capsFilter, "caps", caps, nullptr);
        gst_caps_unref(caps);

        g_object_set(sink,
                     "max-buffers", 1,
                     "drop", TRUE,
                     "sync", FALSE,
                     "emit-signals", FALSE,
                     nullptr);

        gst_bin_add_many(GST_BIN(m_pipeline), src, queue, convert, capsFilter, sink, nullptr);
        if (!gst_element_link_many(src, queue, convert, capsFilter, sink, nullptr)) {
            LOG_ERROR("Failed to link GStreamer capture pipeline");
            stop();
            return false;
        }

        m_appSink = GST_APP_SINK(sink);
        GstStateChangeReturn state = gst_element_set_state(m_pipeline, GST_STATE_PLAYING);
        if (state == GST_STATE_CHANGE_FAILURE) {
            LOG_ERROR("Failed to start GStreamer capture pipeline");
            stop();
            return false;
        }

        LOG_INFO("GStreamer PipeWire capture pipeline started");
        return true;
    }

    QImage sampleToImage(GstSample* sample) {
        GstCaps* caps = gst_sample_get_caps(sample);
        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (!caps || !buffer)
            return {};

        GstVideoInfo info;
        if (!gst_video_info_from_caps(&info, caps)) {
            LOG_ERROR("Failed to parse GStreamer video caps");
            return {};
        }

        GstVideoFrame frame;
        if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {
            LOG_ERROR("Failed to map GStreamer video frame");
            return {};
        }

        const int width = GST_VIDEO_FRAME_WIDTH(&frame);
        const int height = GST_VIDEO_FRAME_HEIGHT(&frame);
        const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);
        const uchar* data = static_cast<const uchar*>(GST_VIDEO_FRAME_PLANE_DATA(&frame, 0));

        QImage image(data, width, height, stride, QImage::Format_BGR888);
        QImage copy = image.copy();
        gst_video_frame_unmap(&frame);
        return copy;
    }

    void stop() {
        if (m_pipeline) {
            gst_element_set_state(m_pipeline, GST_STATE_NULL);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            m_appSink = nullptr;
        }
        if (m_pipeWireFd >= 0) {
            ::close(m_pipeWireFd);
            m_pipeWireFd = -1;
        }
    }

    GstElement* m_pipeline = nullptr;
    GstAppSink* m_appSink = nullptr;
    int m_pipeWireFd = -1;
    std::string m_targetObject;
    bool m_failed = false;
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
        // 1. XDG Screenshot portal — works on GNOME Wayland (no user dialog needed after first accept)
        if (wayland && *wayland) {
            QImage portalImg = captureWithScreenshotPortal();
            if (!portalImg.isNull() && isFullRes(portalImg))
                return portalImg;
        }

        // 2. PipeWire ScreenCast portal (continuous stream — needs user accept)
        if (wayland && *wayland && !m_portalCapture.hasFailed()) {
            QImage portalImage = m_portalCapture.captureFrame();
            if (!portalImage.isNull() && isFullRes(portalImage))
                return portalImage;
        }

        // 3. grim (wlroots compositors only — not GNOME)
        if (wayland && *wayland) {
            QImage img = captureGrim(wayland);
            if (!img.isNull()) return img;
        }

        // 4. GNOME Shell D-Bus (deprecated, often blocked)
        if (QImage gnomeImage = captureWithGnomeShell(); !gnomeImage.isNull())
            return gnomeImage;

        // 5. Qt grabWindow (works on real X11, returns black on XWayland root)
        if (QImage qtImage = captureWithQt(screenIndex); !isBlack(qtImage))
            return qtImage;

        // 6. XGetImage (fails on XWayland root, works on pure X11)
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
    PortalGStreamerCapture m_portalCapture;

    static bool isCurrentSessionGnomeWayland() {
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        const char* sessionType = std::getenv("XDG_SESSION_TYPE");
        const bool isWayland = sessionType && std::strcmp(sessionType, "wayland") == 0;
        const bool isGnome = desktop && std::string(desktop).find("GNOME") != std::string::npos;
        return isWayland && isGnome;
    }

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

    QImage captureWithGnomeShell() {
#ifdef Q_OS_LINUX
        if (!isCurrentSessionGnomeWayland())
            return {};

        QTemporaryFile temp(QDir::tempPath() + "/chessbotx-capture-XXXXXX.png");
        temp.setAutoRemove(false);
        if (!temp.open()) {
            LOG_ERROR("Failed to create temporary screenshot file");
            return {};
        }
        const QString path = temp.fileName();
        temp.close();
        QFile::remove(path);

        QDBusInterface screenshot(
            "org.gnome.Shell.Screenshot",
            "/org/gnome/Shell/Screenshot",
            "org.gnome.Shell.Screenshot"
        );
        if (!screenshot.isValid()) {
            LOG_WARN("GNOME Shell screenshot DBus interface is not available");
            return {};
        }

        QDBusMessage reply = screenshot.call("Screenshot", false, false, path);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            LOG_ERROR("GNOME Shell screenshot failed: {}", reply.errorMessage().toStdString());
            QFile::remove(path);
            return {};
        }

        bool success = false;
        if (!reply.arguments().isEmpty())
            success = reply.arguments().at(0).toBool();

        if (!success) {
            LOG_ERROR("GNOME Shell screenshot returned success=false");
            QFile::remove(path);
            return {};
        }

        QImage image(path);
        QFile::remove(path);
        if (image.isNull()) {
            LOG_ERROR("GNOME Shell screenshot produced an unreadable image");
            return {};
        }

        LOG_TRACE("Captured screen with GNOME Shell DBus backend: {}x{}",
                  image.width(), image.height());
        return image;
#else
        return {};
#endif
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
