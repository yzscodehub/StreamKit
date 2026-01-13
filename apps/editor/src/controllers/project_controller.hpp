/**
 * @file project_controller.hpp
 * @brief Project management controller for QML
 * 
 * Handles project-level operations: create, open, save,
 * media import, and undo/redo.
 */

#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QStringList>
#include <QVariantList>
#include <memory>

namespace phoenix::model {
    class Project;
    class UndoStack;
}

namespace phoenix::editor {

/**
 * @brief Media item for QML binding
 */
struct MediaItemInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(qint64 duration MEMBER duration)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(int height MEMBER height)
    Q_PROPERTY(QString thumbnail MEMBER thumbnail)
public:
    QString id;
    QString name;
    QString path;
    qint64 duration = 0;
    int width = 0;
    int height = 0;
    QString thumbnail;
};

/**
 * @brief Project controller for QML
 * 
 * Exposes project operations to QML including:
 * - New/Open/Save project
 * - Import media files
 * - Undo/Redo
 * - Project settings
 */
class ProjectController : public QObject {
    Q_OBJECT
    
    // Project state
    Q_PROPERTY(QString projectName READ projectName NOTIFY projectChanged)
    Q_PROPERTY(QString projectPath READ projectPath NOTIFY projectChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(bool hasProject READ hasProject NOTIFY projectChanged)
    
    // Media bin
    Q_PROPERTY(QVariantList mediaItems READ mediaItems NOTIFY mediaItemsChanged)
    
    // Undo/Redo
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    
    // Settings
    Q_PROPERTY(int frameWidth READ frameWidth NOTIFY settingsChanged)
    Q_PROPERTY(int frameHeight READ frameHeight NOTIFY settingsChanged)
    Q_PROPERTY(double frameRate READ frameRate NOTIFY settingsChanged)

public:
    explicit ProjectController(QObject* parent = nullptr);
    ~ProjectController() override;
    
    // ========== Project Operations ==========
    
    Q_INVOKABLE void newProject();
    Q_INVOKABLE bool openProject(const QUrl& url);
    Q_INVOKABLE bool saveProject();
    Q_INVOKABLE bool saveProjectAs(const QUrl& url);
    Q_INVOKABLE void closeProject();
    
    // ========== Media Import ==========
    
    Q_INVOKABLE void importMedia(const QUrl& url);
    Q_INVOKABLE void importMediaFiles(const QList<QUrl>& urls);
    Q_INVOKABLE void removeMediaItem(const QString& id);
    
    // ========== Undo/Redo ==========
    
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    
    // ========== Property Getters ==========
    
    QString projectName() const;
    QString projectPath() const;
    bool isModified() const;
    bool hasProject() const;
    
    QVariantList mediaItems() const;
    
    bool canUndo() const;
    bool canRedo() const;
    QString undoText() const;
    QString redoText() const;
    
    int frameWidth() const;
    int frameHeight() const;
    double frameRate() const;
    
    // ========== Internal Access ==========
    
    model::Project* project() const { return m_project.get(); }
    model::UndoStack* undoStack() const { return m_undoStack.get(); }

signals:
    void projectChanged();
    void modifiedChanged();
    void mediaItemsChanged();
    void undoStateChanged();
    void settingsChanged();
    
    void errorOccurred(const QString& message);
    void statusMessage(const QString& message);

private:
    void setupConnections();
    void updateMediaItems();
    
    std::unique_ptr<model::Project> m_project;
    std::unique_ptr<model::UndoStack> m_undoStack;
    QString m_projectPath;
    QVariantList m_mediaItems;
};

} // namespace phoenix::editor
