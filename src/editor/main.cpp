/**
 * @file main.cpp
 * @brief PhoenixEditor QML entry point
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QIcon>
#include <QDirIterator>
#include <QQmlError>
#include <spdlog/spdlog.h>

#include "videocontroller.hpp"

int main(int argc, char *argv[]) {
    // Configure logging
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    
    spdlog::info("PhoenixEditor v0.1.0 (QML)");
    
    // Create Qt application
    QGuiApplication app(argc, argv);
    app.setApplicationName("PhoenixEditor");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("Phoenix");
    app.setWindowIcon(QIcon(":/icons/app.png"));
    
    // Set Material Design style
    QQuickStyle::setStyle("Material");
    
    // Register C++ types for QML
    qmlRegisterType<phoenix::VideoController>("Phoenix", 1, 0, "VideoController");
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Load main QML file
    const QUrl url(u"qrc:/PhoenixEditor/src/editor/qml/Main.qml"_qs);
    spdlog::info("Loading QML from: {}", url.toString().toStdString());
    
    // Add error handler for QML warnings
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
        [](const QList<QQmlError>& warnings) {
            for (const auto& warning : warnings) {
                spdlog::warn("QML: {}", warning.toString().toStdString());
            }
        });
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { 
            spdlog::error("QML object creation failed!");
            QCoreApplication::exit(-1); 
        },
        Qt::QueuedConnection);
    
    engine.load(url);
    
    if (engine.rootObjects().isEmpty()) {
        spdlog::error("Failed to load QML from: {}", url.toString().toStdString());
        // Try to list available resources
        QDirIterator it(":", QDirIterator::Subdirectories);
        spdlog::debug("Available resources:");
        int count = 0;
        while (it.hasNext() && count < 30) {
            spdlog::debug("  {}", it.next().toStdString());
            count++;
        }
        return -1;
    }
    
    spdlog::info("Editor started");
    
    return app.exec();
}
