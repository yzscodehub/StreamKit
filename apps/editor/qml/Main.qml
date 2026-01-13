import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import "components"

/**
 * Phoenix Editor Main Window
 * 
 * Layout:
 * ┌─────────────────────────────────────────────────────────────┐
 * │                        Toolbar                               │
 * ├──────────────┬───────────────────────────┬──────────────────┤
 * │              │                           │                  │
 * │  MediaBrowser│     VideoPreview          │  PropertyPanel   │
 * │              │                           │                  │
 * │              ├───────────────────────────┤                  │
 * │              │   TransportControls       │                  │
 * ├──────────────┴───────────────────────────┴──────────────────┤
 * │                                                              │
 * │                        Timeline                              │
 * │                                                              │
 * └──────────────────────────────────────────────────────────────┘
 */
ApplicationWindow {
    id: root
    
    width: 1600
    height: 900
    minimumWidth: 1280
    minimumHeight: 720
    visible: true
    
    title: {
        let name = ProjectController.projectName || "Untitled"
        let modified = ProjectController.isModified ? " •" : ""
        return `${name}${modified} — Phoenix Editor`
    }
    
    color: Theme.bg1
    
    // Force dark palette for all controls
    palette {
        window: Theme.bg1
        windowText: Theme.textPrimary
        base: Theme.bg2
        alternateBase: Theme.bg3
        text: Theme.textPrimary
        button: Theme.bg4
        buttonText: Theme.textPrimary
        highlight: Theme.accent
        highlightedText: Theme.textPrimary
        mid: Theme.bg3
        dark: Theme.bg1
        light: Theme.bg5
    }
    
    // ========== Menu Bar ==========
    
    menuBar: MenuBar {
        background: Rectangle { color: Theme.bg2 }
        
        Menu {
            title: qsTr("&File")
            
            Action {
                text: qsTr("&New Project")
                shortcut: "Ctrl+N"
                onTriggered: ProjectController.newProject()
            }
            Action {
                text: qsTr("&Open Project...")
                shortcut: "Ctrl+O"
                onTriggered: openDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Save")
                shortcut: "Ctrl+S"
                enabled: ProjectController.isModified
                onTriggered: ProjectController.saveProject()
            }
            Action {
                text: qsTr("Save &As...")
                shortcut: "Ctrl+Shift+S"
                onTriggered: saveDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Import Media...")
                shortcut: "Ctrl+I"
                onTriggered: importDialog.open()
            }
            MenuSeparator {}
            Action {
                text: qsTr("E&xit")
                shortcut: "Alt+F4"
                onTriggered: Qt.quit()
            }
        }
        
        Menu {
            title: qsTr("&Edit")
            
            Action {
                text: qsTr("&Undo") + (ProjectController.undoText ? ` "${ProjectController.undoText}"` : "")
                shortcut: "Ctrl+Z"
                enabled: ProjectController.canUndo
                onTriggered: ProjectController.undo()
            }
            Action {
                text: qsTr("&Redo") + (ProjectController.redoText ? ` "${ProjectController.redoText}"` : "")
                shortcut: "Ctrl+Y"
                enabled: ProjectController.canRedo
                onTriggered: ProjectController.redo()
            }
            MenuSeparator {}
            Action {
                text: qsTr("&Delete")
                shortcut: "Delete"
                onTriggered: TimelineController.deleteSelectedClip()
            }
            Action {
                text: qsTr("&Split at Playhead")
                shortcut: "S"
                onTriggered: {
                    if (TimelineController.selectedClipId) {
                        TimelineController.splitClipAtPlayhead(TimelineController.selectedClipId)
                    }
                }
            }
        }
        
        Menu {
            title: qsTr("&View")
            
            Action {
                text: qsTr("Zoom &In")
                shortcut: "Ctrl+="
                onTriggered: TimelineController.zoomIn()
            }
            Action {
                text: qsTr("Zoom &Out")
                shortcut: "Ctrl+-"
                onTriggered: TimelineController.zoomOut()
            }
            Action {
                text: qsTr("Zoom to &Fit")
                shortcut: "Ctrl+0"
                onTriggered: TimelineController.zoomToFit()
            }
        }
        
        Menu {
            title: qsTr("&Playback")
            
            Action {
                text: PreviewController.isPlaying ? qsTr("&Pause") : qsTr("&Play")
                shortcut: "Space"
                onTriggered: PreviewController.togglePlayPause()
            }
            Action {
                text: qsTr("&Stop")
                shortcut: "Escape"
                onTriggered: PreviewController.stop()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Go to &Start")
                shortcut: "Home"
                onTriggered: PreviewController.goToStart()
            }
            Action {
                text: qsTr("Go to &End")
                shortcut: "End"
                onTriggered: PreviewController.goToEnd()
            }
            MenuSeparator {}
            Action {
                text: qsTr("Step &Forward")
                shortcut: "Right"
                onTriggered: PreviewController.stepForward()
            }
            Action {
                text: qsTr("Step &Backward")
                shortcut: "Left"
                onTriggered: PreviewController.stepBackward()
            }
        }
        
        Menu {
            title: qsTr("&Help")
            
            Action {
                text: qsTr("&About Phoenix Editor")
                onTriggered: aboutDialog.open()
            }
        }
    }
    
    // ========== Main Layout ==========
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Toolbar
        Toolbar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.toolbarHeight
        }
        
        // Main content area
        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal
            
            handle: Rectangle {
                implicitWidth: 4
                color: SplitHandle.pressed ? Theme.accent : 
                       SplitHandle.hovered ? Theme.borderLight : Theme.border
            }
            
            // Left panel - Media Browser
            MediaBrowser {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 400
            }
            
            // Center - Preview + Transport
            ColumnLayout {
                SplitView.fillWidth: true
                SplitView.minimumWidth: 500
                spacing: 0
                
                // Video Preview
                VideoPreview {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }
                
                // Transport Controls
                TransportControls {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.transportHeight
                }
            }
            
            // Right panel - Properties
            PropertyPanel {
                SplitView.preferredWidth: 280
                SplitView.minimumWidth: 200
                SplitView.maximumWidth: 400
            }
        }
        
        // Timeline
        Timeline {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            Layout.minimumHeight: 150
            Layout.maximumHeight: 400
        }
    }
    
    // ========== Dialogs ==========
    
    FileDialog {
        id: openDialog
        title: qsTr("Open Project")
        nameFilters: ["Phoenix Project (*.phoenix)", "All files (*)"]
        onAccepted: ProjectController.openProject(selectedFile)
    }
    
    FileDialog {
        id: saveDialog
        title: qsTr("Save Project As")
        fileMode: FileDialog.SaveFile
        nameFilters: ["Phoenix Project (*.phoenix)"]
        onAccepted: ProjectController.saveProjectAs(selectedFile)
    }
    
    FileDialog {
        id: importDialog
        title: qsTr("Import Media")
        fileMode: FileDialog.OpenFiles
        nameFilters: [
            "Video files (*.mp4 *.mov *.avi *.mkv *.webm)",
            "Audio files (*.mp3 *.wav *.aac *.flac)",
            "All files (*)"
        ]
        onAccepted: ProjectController.importMediaFiles(selectedFiles)
    }
    
    Dialog {
        id: aboutDialog
        title: qsTr("About Phoenix Editor")
        modal: true
        anchors.centerIn: parent
        width: 400
        
        background: Rectangle {
            color: Theme.bg3
            radius: Theme.radiusLg
            border.color: Theme.border
        }
        
        ColumnLayout {
            anchors.fill: parent
            spacing: Theme.spacingMd
            
            Text {
                text: "Phoenix Editor"
                font.pixelSize: Theme.fontSizeXxl
                font.weight: Theme.fontWeightBold
                color: Theme.textPrimary
            }
            
            Text {
                text: "Version 0.4.0"
                font.pixelSize: Theme.fontSizeMd
                color: Theme.textSecondary
            }
            
            Text {
                text: "A modern, non-linear video editor built with C++20 and Qt 6."
                font.pixelSize: Theme.fontSizeMd
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            
            Text {
                text: "© 2026 Phoenix Project"
                font.pixelSize: Theme.fontSizeSm
                color: Theme.textDim
            }
        }
        
        standardButtons: Dialog.Ok
    }
    
    // ========== Status Bar Messages ==========
    
    Connections {
        target: ProjectController
        
        function onStatusMessage(message) {
            statusText.text = message
            statusTimer.restart()
        }
        
        function onErrorOccurred(message) {
            errorDialog.text = message
            errorDialog.open()
        }
    }
    
    // Status toast
    Rectangle {
        id: statusToast
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 100
        
        width: statusText.width + Theme.spacingLg * 2
        height: statusText.height + Theme.spacingSm * 2
        radius: Theme.radiusMd
        color: Theme.bg4
        opacity: statusTimer.running ? 1.0 : 0.0
        visible: opacity > 0
        
        Behavior on opacity { NumberAnimation { duration: Theme.animNormal } }
        
        Text {
            id: statusText
            anchors.centerIn: parent
            font.pixelSize: Theme.fontSizeSm
            color: Theme.textPrimary
        }
        
        Timer {
            id: statusTimer
            interval: 3000
        }
    }
    
    Dialog {
        id: errorDialog
        title: qsTr("Error")
        modal: true
        anchors.centerIn: parent
        
        property alias text: errorText.text
        
        background: Rectangle {
            color: Theme.bg3
            radius: Theme.radiusLg
            border.color: Theme.error
        }
        
        Text {
            id: errorText
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            wrapMode: Text.WordWrap
        }
        
        standardButtons: Dialog.Ok
    }
    
    // ========== Keyboard Shortcuts ==========
    
    Shortcut {
        sequence: "Space"
        onActivated: PreviewController.togglePlayPause()
    }
    
    Shortcut {
        sequence: "J"
        onActivated: {
            let speed = PreviewController.playbackSpeed
            PreviewController.setPlaybackSpeed(Math.max(0.25, speed / 2))
        }
    }
    
    Shortcut {
        sequence: "L"
        onActivated: {
            let speed = PreviewController.playbackSpeed
            PreviewController.setPlaybackSpeed(Math.min(4.0, speed * 2))
        }
    }
    
    Shortcut {
        sequence: "K"
        onActivated: PreviewController.pause()
    }
}
