cmake_minimum_required(VERSION 3.25)
project(chessbotx VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus)
find_package(OpenCV REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(spdlog REQUIRED)
find_package(PkgConfig REQUIRED)

# X11 / XTest for Linux mouse simulation
if(UNIX AND NOT APPLE)
    pkg_check_modules(XTST REQUIRED xtst)
    pkg_check_modules(X11  REQUIRED x11)
    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0)
endif()

add_executable(chessbotx
    src/main.cpp
    src/app/Application.cpp
    src/chess/Board.cpp
    src/chess/Fen.cpp
    src/chess/PositionValidator.cpp
    src/engine/StockfishProcess.cpp
    src/engine/UciParser.cpp
    src/vision/ScreenCapture.cpp
    src/vision/BoardCalibration.cpp
    src/vision/BoardDetector.cpp
    src/vision/FrameStabilityDetector.cpp
    src/vision/TemplatePieceRecognizer.cpp
    src/ui/OverlayWidget.cpp
    src/ui/SettingsWindow.cpp
    src/ui/CalibrationOverlay.cpp
    src/ui/StatusWindow.cpp
    src/platform/DesktopCapture.cpp
    src/infrastructure/Configuration.cpp
    src/infrastructure/Logger.cpp
)

target_include_directories(chessbotx PRIVATE
    src
    third_party
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(chessbotx PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

if(UNIX AND NOT APPLE)
    target_include_directories(chessbotx PRIVATE
        ${XTST_INCLUDE_DIRS}
        ${X11_INCLUDE_DIRS}
        ${GSTREAMER_INCLUDE_DIRS}
    )
    target_link_libraries(chessbotx PRIVATE
        ${XTST_LIBRARIES}
        ${X11_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
    )
endif()

# Copy templates from assets to build directory
add_custom_command(TARGET chessbotx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets/templates
        $<TARGET_FILE_DIR:chessbotx>/templates
    COMMENT "Copying piece templates to build directory"
)

# Tests
enable_testing()
add_subdirectory(tests)
