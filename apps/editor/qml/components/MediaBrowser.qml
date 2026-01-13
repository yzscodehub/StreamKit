import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Media browser panel - shows imported media items
 */
Rectangle {
    id: root
    color: Theme.bg2
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.bg3
            
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingSm
                
                Text {
                    text: qsTr("Media")
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Theme.fontWeightMedium
                    color: Theme.textPrimary
                }
                
                Item { Layout.fillWidth: true }
                
                ToolButton {
                    icon.source: "qrc:/Phoenix/resources/icons/import.svg"
                    icon.width: Theme.iconSizeSm
                    icon.height: Theme.iconSizeSm
                    ToolTip.text: qsTr("Import Media")
                    ToolTip.visible: hovered
                    onClicked: importDialog.open()
                }
            }
            
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }
        
        // Search box
        TextField {
            id: searchField
            Layout.fillWidth: true
            Layout.margins: Theme.spacingSm
            placeholderText: qsTr("Search media...")
            
            background: Rectangle {
                color: Theme.bg4
                radius: Theme.radiusSm
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
            }
            
            color: Theme.textPrimary
            placeholderTextColor: Theme.textDim
            font.pixelSize: Theme.fontSizeSm
        }
        
        // Media list
        ListView {
            id: mediaList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            model: ProjectController.mediaItems
            
            delegate: Rectangle {
                width: mediaList.width
                height: 64
                color: mouseArea.containsMouse ? Theme.bg4 : "transparent"
                
                property var item: modelData
                
                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingSm
                    spacing: Theme.spacingSm
                    
                    // Thumbnail placeholder
                    Rectangle {
                        Layout.preferredWidth: 80
                        Layout.preferredHeight: 48
                        color: Theme.bg5
                        radius: Theme.radiusSm
                        
                        // Video icon placeholder
                        Text {
                            anchors.centerIn: parent
                            text: item.hasVideo ? "🎬" : "🎵"
                            font.pixelSize: 20
                        }
                    }
                    
                    // Info
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        
                        Text {
                            text: item.name
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Theme.fontWeightMedium
                            color: Theme.textPrimary
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        
                        Text {
                            text: {
                                let dur = item.duration / 1000000
                                let min = Math.floor(dur / 60)
                                let sec = Math.floor(dur % 60)
                                return `${min}:${sec.toString().padStart(2, '0')}`
                            }
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.monoFont
                            color: Theme.textDim
                        }
                        
                        Text {
                            text: item.hasVideo ? `${item.width}×${item.height}` : "Audio"
                            font.pixelSize: Theme.fontSizeXs
                            color: Theme.textDim
                        }
                    }
                }
                
                MouseArea {
                    id: mouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    
                    drag.target: dragItem
                    
                    onPressed: (mouse) => {
                        dragItem.item = item
                        dragItem.x = mouse.x - 40
                        dragItem.y = mouse.y - 24
                        dragItem.visible = true
                    }
                    
                    onReleased: {
                        dragItem.visible = false
                        dragItem.Drag.drop()
                    }
                    
                    onDoubleClicked: {
                        // Add to timeline at playhead
                        if (TimelineController.videoTrackCount > 0) {
                            let tracks = TimelineController.videoTracks
                            if (tracks.length > 0) {
                                TimelineController.addClipAtPlayhead(
                                    item.id, tracks[0].id)
                            }
                        }
                    }
                }
            }
            
            // Empty state
            Text {
                anchors.centerIn: parent
                visible: mediaList.count === 0
                text: qsTr("No media imported\n\nDrag files here or use\nFile → Import Media")
                font.pixelSize: Theme.fontSizeSm
                color: Theme.textDim
                horizontalAlignment: Text.AlignHCenter
            }
            
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
        }
    }
    
    // Drag item
    Rectangle {
        id: dragItem
        visible: false
        width: 80
        height: 48
        color: Theme.clipVideo
        radius: Theme.radiusSm
        opacity: 0.8
        
        property var item: null
        
        Drag.active: visible
        Drag.keys: ["media"]
        Drag.mimeData: { "mediaId": item ? item.id : "" }
        
        Text {
            anchors.centerIn: parent
            text: dragItem.item ? dragItem.item.name : ""
            font.pixelSize: Theme.fontSizeXs
            color: "white"
            elide: Text.ElideMiddle
            width: parent.width - 8
            horizontalAlignment: Text.AlignHCenter
        }
    }
    
    // Right border
    Rectangle {
        anchors.right: parent.right
        width: 1
        height: parent.height
        color: Theme.border
    }
}
