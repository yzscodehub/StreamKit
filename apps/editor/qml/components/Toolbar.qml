import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Main toolbar with common actions
 */
Rectangle {
    id: root
    color: Theme.bg2
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingMd
        anchors.rightMargin: Theme.spacingMd
        spacing: Theme.spacingSm
        
        // Project actions
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/import.svg"
            ToolTip.text: qsTr("Import Media (Ctrl+I)")
            ToolTip.visible: hovered
            onClicked: importDialog.open()
        }
        
        ToolSeparator {}
        
        // Undo/Redo
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/undo.svg"
            enabled: ProjectController.canUndo
            ToolTip.text: qsTr("Undo (Ctrl+Z)")
            ToolTip.visible: hovered
            onClicked: ProjectController.undo()
        }
        
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/redo.svg"
            enabled: ProjectController.canRedo
            ToolTip.text: qsTr("Redo (Ctrl+Y)")
            ToolTip.visible: hovered
            onClicked: ProjectController.redo()
        }
        
        ToolSeparator {}
        
        // Edit tools
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/cut.svg"
            ToolTip.text: qsTr("Split at Playhead (S)")
            ToolTip.visible: hovered
            onClicked: {
                if (TimelineController.selectedClipId) {
                    TimelineController.splitClipAtPlayhead(TimelineController.selectedClipId)
                }
            }
        }
        
        ToolSeparator {}
        
        // Zoom
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/zoom-out.svg"
            ToolTip.text: qsTr("Zoom Out (Ctrl+-)")
            ToolTip.visible: hovered
            onClicked: TimelineController.zoomOut()
        }
        
        Slider {
            id: zoomSlider
            Layout.preferredWidth: 100
            from: 10
            to: 500
            value: TimelineController.pixelsPerSecond
            onMoved: TimelineController.pixelsPerSecond = value
            
            background: Rectangle {
                x: zoomSlider.leftPadding
                y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                width: zoomSlider.availableWidth
                height: 4
                radius: 2
                color: Theme.bg4
                
                Rectangle {
                    width: zoomSlider.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: Theme.accent
                }
            }
            
            handle: Rectangle {
                x: zoomSlider.leftPadding + zoomSlider.visualPosition * (zoomSlider.availableWidth - width)
                y: zoomSlider.topPadding + zoomSlider.availableHeight / 2 - height / 2
                width: 12
                height: 12
                radius: 6
                color: zoomSlider.pressed ? Theme.accentHover : Theme.accent
            }
        }
        
        ToolButton {
            icon.source: "qrc:/Phoenix/resources/icons/zoom-in.svg"
            ToolTip.text: qsTr("Zoom In (Ctrl+=)")
            ToolTip.visible: hovered
            onClicked: TimelineController.zoomIn()
        }
        
        // Spacer
        Item { Layout.fillWidth: true }
        
        // Snap toggle
        ToolButton {
            text: qsTr("Snap")
            checkable: true
            checked: TimelineController.snapEnabled
            onClicked: TimelineController.snapEnabled = checked
            
            background: Rectangle {
                color: parent.checked ? Theme.accentBg : 
                       parent.hovered ? Theme.bg5 : "transparent"
                radius: Theme.radiusSm
                border.color: parent.checked ? Theme.accent : "transparent"
            }
            
            contentItem: Text {
                text: parent.text
                font.pixelSize: Theme.fontSizeSm
                color: parent.checked ? Theme.accent : Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
        
        // Project info
        Text {
            text: `${ProjectController.frameWidth}×${ProjectController.frameHeight} @ ${ProjectController.frameRate.toFixed(2)} fps`
            font.pixelSize: Theme.fontSizeSm
            font.family: Theme.monoFont
            color: Theme.textDim
        }
    }
    
    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
}
