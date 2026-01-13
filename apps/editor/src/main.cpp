/**
 * @file main.cpp
 * @brief Phoenix Editor entry point
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QIcon>
#include <QPalette>

#include <phoenix/core/logger.hpp>

#include "controllers/project_controller.hpp"
#include "controllers/timeline_controller.hpp"
#include "controllers/preview_controller.hpp"
#include "theme.hpp"

int main(int argc, char* argv[]) {
    // Initialize logging
    phoenix::initLogging("PhoenixEditor", spdlog::level::debug);
    
    // Qt application setup
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Phoenix");
    app.setOrganizationDomain("phoenix.dev");
    app.setApplicationName("Phoenix Editor");
    app.setApplicationVersion("0.4.0");
    
    // Set dark palette for native controls
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(0x0d, 0x0d, 0x0d));
    darkPalette.setColor(QPalette::WindowText, QColor(0xf0, 0xf0, 0xf0));
    darkPalette.setColor(QPalette::Base, QColor(0x14, 0x14, 0x14));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x1a, 0x1a, 0x1a));
    darkPalette.setColor(QPalette::Text, QColor(0xf0, 0xf0, 0xf0));
    darkPalette.setColor(QPalette::Button, QColor(0x22, 0x22, 0x22));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xf0, 0xf0, 0xf0));
    darkPalette.setColor(QPalette::Highlight, QColor(0x00, 0xd4, 0xaa));
    darkPalette.setColor(QPalette::HighlightedText, QColor(0xf0, 0xf0, 0xf0));
    app.setPalette(darkPalette);
    
    // Set application style - Basic for custom styling
    QQuickStyle::setStyle("Basic");
    
    // Create theme (must be created before QML engine)
    phoenix::editor::Theme theme;
    
    // Create controllers
    phoenix::editor::ProjectController projectController;
    phoenix::editor::TimelineController timelineController(&projectController);
    phoenix::editor::PreviewController previewController(&projectController, 
                                                          &timelineController);
    
    // QML engine
    QQmlApplicationEngine engine;
    
    // Register image provider
    engine.addImageProvider("preview", previewController.imageProvider());
    
    // Expose theme and controllers to QML as context properties
    QQmlContext* context = engine.rootContext();
    context->setContextProperty("Theme", &theme);
    context->setContextProperty("ProjectController", &projectController);
    context->setContextProperty("TimelineController", &timelineController);
    context->setContextProperty("PreviewController", &previewController);
    
    // Load main QML  
    // Resource path based on qmldir: prefer :/Phoenix/
    const QUrl url(QStringLiteral("qrc:/Phoenix/qml/Main.qml"));
    
    LOG_INFO("Loading QML from: {}", url.toString().toStdString());
    
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { 
                         LOG_ERROR("QML object creation failed");
                         QCoreApplication::exit(-1); 
                     },
                     Qt::QueuedConnection);
    
    QObject::connect(&engine, &QQmlApplicationEngine::warnings,
                     [](const QList<QQmlError>& warnings) {
                         for (const auto& warning : warnings) {
                             LOG_WARN("QML Warning: {}", warning.toString().toStdString());
                         }
                     });
    
    engine.load(url);
    
    if (engine.rootObjects().isEmpty()) {
        LOG_ERROR("Failed to load QML - rootObjects is empty");
        
        // Try alternative paths
        QStringList paths = {
            "qrc:/Phoenix/qml/Main.qml",
            "qrc:/qml/Main.qml",
            "qrc:/PhoenixEditor/qml/Main.qml"
        };
        
        for (const auto& path : paths) {
            LOG_INFO("Trying alternative path: {}", path.toStdString());
        }
        
        return -1;
    }
    
    LOG_INFO("Phoenix Editor started");
    
    return app.exec();
}
