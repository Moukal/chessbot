#include "app/Application.hpp"
#include "infrastructure/Logger.hpp"
#include <QApplication>
#include <QDir>
#include <QStandardPaths>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("chessbotx");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("chessbotx");
    app.setQuitOnLastWindowClosed(false);

    Logger::init("chessbotx.log");
    LOG_INFO("ChessBotX v0.1.0 starting");

    Application chessBotApp;
    if (!chessBotApp.init()) {
        LOG_CRITICAL("Initialization failed");
        return 1;
    }

    return app.exec();
}
