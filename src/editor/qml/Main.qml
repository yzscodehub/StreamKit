import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs
import Phoenix 1.0

ApplicationWindow {
    id: root
    width: 1600
    height: 900
    minimumWidth: 1280
    minimumHeight: 720
    visible: true
    title: qsTr("Phoenix Editor")
    
    // Dark theme
    Material.theme: Material.Dark
    Material.accent: Material.Teal
    Material.primary: Material.BlueGrey
    
    // Video controller backend
    VideoController {
        id: videoController
        onError: (message) => {
            errorDialog.text = message
            errorDialog.open()
        }
    }
    
    // Background
    color: "#1a1a2e"
    
    // Menu Bar
    menuBar: MenuBar {
        Menu {
            title: qsTr("&File")
            Action { 
                text: qsTr("&Open...")
                shortcut: "Ctrl+O"
                onTriggered: fileDialog.open()
            }
            MenuSeparator {}
            Action { 
                text: qsTr("&Export...")
                shortcut: "Ctrl+E"
                enabled: videoController.source !== ""
            }
            MenuSeparator {}
            Action { 
                text: qsTr("E&xit")
                shortcut: "Ctrl+Q"
                onTriggered: Qt.quit()
            }
        }
        Menu {
            title: qsTr("&Edit")
            Action { text: qsTr("&Undo"); shortcut: "Ctrl+Z" }
            Action { text: qsTr("&Redo"); shortcut: "Ctrl+Y" }
            MenuSeparator {}
            Action { text: qsTr("Cu&t"); shortcut: "Ctrl+X" }
            Action { text: qsTr("&Copy"); shortcut: "Ctrl+C" }
            Action { text: qsTr("&Paste"); shortcut: "Ctrl+V" }
        }
        Menu {
            title: qsTr("&View")
            Action { text: qsTr("&Fullscreen"); shortcut: "F11"; checkable: true }
        }
        Menu {
            title: qsTr("&Help")
            Action { text: qsTr("&About") }
        }
    }
    
    // Main layout
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Top area: Video Preview + Sidebar
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            
            // Video Preview (center)
            VideoPreview {
                id: videoPreview
                Layout.fillWidth: true
                Layout.fillHeight: true
                controller: videoController
            }
            
            // Right Sidebar
            Sidebar {
                id: sidebar
                Layout.preferredWidth: 300
                Layout.fillHeight: true
                controller: videoController
            }
        }
        
        // Transport Controls
        TransportControls {
            id: transportControls
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            controller: videoController
        }
        
        // Timeline
        Timeline {
            id: timeline
            Layout.fillWidth: true
            Layout.preferredHeight: 200
            controller: videoController
        }
    }
    
    // File dialog
    FileDialog {
        id: fileDialog
        title: qsTr("Open Video File")
        nameFilters: ["Video files (*.mp4 *.mkv *.avi *.mov *.webm)", "All files (*)"]
        onAccepted: {
            videoController.source = selectedFile
        }
    }
    
    // Error dialog
    Dialog {
        id: errorDialog
        property alias text: errorLabel.text
        title: qsTr("Error")
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        
        Label {
            id: errorLabel
            wrapMode: Text.Wrap
        }
    }
    
    // Keyboard shortcuts
    Shortcut {
        sequence: "Space"
        onActivated: videoController.togglePlayPause()
    }
    
    Shortcut {
        sequence: "Left"
        onActivated: videoController.seek(videoController.position - 5000)
    }
    
    Shortcut {
        sequence: "Right"
        onActivated: videoController.seek(videoController.position + 5000)
    }
}


