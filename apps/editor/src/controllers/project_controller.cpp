/**
 * @file project_controller.cpp
 * @brief Project controller implementation
 */

#include "project_controller.hpp"

#include <phoenix/model/project.hpp>
#include <phoenix/model/media_item.hpp>
#include <phoenix/model/media_bin.hpp>
#include <phoenix/model/commands/undo_stack.hpp>
#include <phoenix/media/media_info.hpp>
#include <phoenix/core/logger.hpp>

#include <QFileInfo>
#include <QDir>

#include <filesystem>

namespace phoenix::editor {

ProjectController::ProjectController(QObject* parent)
    : QObject(parent)
    , m_undoStack(std::make_unique<model::UndoStack>())
{
    setupConnections();
    newProject();  // Start with empty project
}

ProjectController::~ProjectController() = default;

void ProjectController::setupConnections() {
    // Connect undo stack signals (ignore returned Connection)
    (void)m_undoStack->indexChanged.connect([this]() {
        emit undoStateChanged();
        emit modifiedChanged();
    });
    
    (void)m_undoStack->cleanChanged.connect([this](bool) {
        emit modifiedChanged();
    });
}

// ============================================================================
// Project Operations
// ============================================================================

void ProjectController::newProject() {
    m_project = std::make_unique<model::Project>();
    m_projectPath.clear();
    m_undoStack->clear();
    
    updateMediaItems();
    
    emit projectChanged();
    emit settingsChanged();
    emit statusMessage(tr("New project created"));
}

bool ProjectController::openProject(const QUrl& url) {
    QString path = url.toLocalFile();
    
    // TODO: Implement JSON deserialization
    // auto result = model::ProjectIO::load(path.toStdString());
    // if (!result) {
    //     emit errorOccurred(tr("Failed to open project: %1").arg(QString::fromStdString(result.error().message)));
    //     return false;
    // }
    
    // m_project = std::move(result.value());
    m_projectPath = path;
    m_undoStack->clear();
    m_undoStack->setClean();
    
    updateMediaItems();
    
    emit projectChanged();
    emit settingsChanged();
    emit statusMessage(tr("Project opened: %1").arg(path));
    
    return true;
}

bool ProjectController::saveProject() {
    if (m_projectPath.isEmpty()) {
        emit errorOccurred(tr("No save path specified"));
        return false;
    }
    
    return saveProjectAs(QUrl::fromLocalFile(m_projectPath));
}

bool ProjectController::saveProjectAs(const QUrl& url) {
    QString path = url.toLocalFile();
    
    // TODO: Implement JSON serialization
    // auto result = model::ProjectIO::save(*m_project, path.toStdString());
    // if (!result) {
    //     emit errorOccurred(tr("Failed to save project: %1").arg(QString::fromStdString(result.error().message)));
    //     return false;
    // }
    
    m_projectPath = path;
    m_undoStack->setClean();
    
    emit projectChanged();
    emit statusMessage(tr("Project saved: %1").arg(path));
    
    return true;
}

void ProjectController::closeProject() {
    m_project.reset();
    m_projectPath.clear();
    m_undoStack->clear();
    m_mediaItems.clear();
    
    emit projectChanged();
    emit mediaItemsChanged();
}

// ============================================================================
// Media Import
// ============================================================================

void ProjectController::importMedia(const QUrl& url) {
    if (!m_project) {
        emit errorOccurred(tr("No project open"));
        return;
    }
    
    QString path = url.toLocalFile();
    QFileInfo fileInfo(path);
    
    if (!fileInfo.exists()) {
        emit errorOccurred(tr("File not found: %1").arg(path));
        return;
    }
    
    // Probe media info using static method
    auto result = media::MediaInfo::probe(std::filesystem::path(path.toStdString()));
    if (!result) {
        emit errorOccurred(tr("Failed to read media file: %1").arg(path));
        return;
    }
    
    // MediaInfo::probe returns a fully populated MediaItem
    auto item = result.value();
    item->setName(fileInfo.fileName().toStdString());
    
    // Add to project
    m_project->mediaBin().addItem(item);
    
    updateMediaItems();
    
    emit statusMessage(tr("Imported: %1").arg(fileInfo.fileName()));
}

void ProjectController::importMediaFiles(const QList<QUrl>& urls) {
    for (const auto& url : urls) {
        importMedia(url);
    }
}

void ProjectController::removeMediaItem(const QString& id) {
    if (!m_project) return;
    
    UUID uuid = UUID::fromString(id.toStdString());
    m_project->mediaBin().removeItem(uuid);
    
    updateMediaItems();
}

// ============================================================================
// Undo/Redo
// ============================================================================

void ProjectController::undo() {
    m_undoStack->undo();
}

void ProjectController::redo() {
    m_undoStack->redo();
}

// ============================================================================
// Property Getters
// ============================================================================

QString ProjectController::projectName() const {
    if (!m_project) return QString();
    return QString::fromStdString(m_project->name());
}

QString ProjectController::projectPath() const {
    return m_projectPath;
}

bool ProjectController::isModified() const {
    return m_undoStack && !m_undoStack->isClean();
}

bool ProjectController::hasProject() const {
    return m_project != nullptr;
}

QVariantList ProjectController::mediaItems() const {
    return m_mediaItems;
}

bool ProjectController::canUndo() const {
    return m_undoStack && m_undoStack->canUndo();
}

bool ProjectController::canRedo() const {
    return m_undoStack && m_undoStack->canRedo();
}

QString ProjectController::undoText() const {
    if (!m_undoStack) return QString();
    return QString::fromStdString(m_undoStack->undoText());
}

QString ProjectController::redoText() const {
    if (!m_undoStack) return QString();
    return QString::fromStdString(m_undoStack->redoText());
}

int ProjectController::frameWidth() const {
    if (!m_project) return 1920;
    auto seq = m_project->activeSequence();
    return seq ? seq->settings().resolution.width : 1920;
}

int ProjectController::frameHeight() const {
    if (!m_project) return 1080;
    auto seq = m_project->activeSequence();
    return seq ? seq->settings().resolution.height : 1080;
}

double ProjectController::frameRate() const {
    if (!m_project) return 30.0;
    auto seq = m_project->activeSequence();
    return seq ? seq->settings().fps() : 30.0;
}

// ============================================================================
// Private Methods
// ============================================================================

void ProjectController::updateMediaItems() {
    m_mediaItems.clear();
    
    if (!m_project) {
        emit mediaItemsChanged();
        return;
    }
    
    for (const auto& item : m_project->mediaBin().items()) {
        QVariantMap map;
        map["id"] = QString::fromStdString(item->id().toString());
        map["name"] = QString::fromStdString(item->name());
        map["path"] = QString::fromStdString(item->path().string());
        map["duration"] = static_cast<qint64>(item->duration());
        map["width"] = item->videoProperties().resolution.width;
        map["height"] = item->videoProperties().resolution.height;
        map["hasVideo"] = item->hasVideo();
        map["hasAudio"] = item->hasAudio();
        
        m_mediaItems.append(map);
    }
    
    emit mediaItemsChanged();
}

} // namespace phoenix::editor
