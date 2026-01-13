import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Video preview panel - displays current frame
 */
Rectangle {
    id: root
    color: Theme.bg1
    
    // Preview container with aspect ratio
    Item {
        id: previewContainer
        anchors.centerIn: parent
        
        // Calculate size maintaining aspect ratio
        property real aspectRatio: PreviewController.previewWidth / 
                                   Math.max(1, PreviewController.previewHeight)
        
        width: {
            let maxW = parent.width - Theme.spacingLg * 2
            let maxH = parent.height - Theme.spacingLg * 2
            let w = maxW
            let h = w / aspectRatio
            if (h > maxH) {
                h = maxH
                w = h * aspectRatio
            }
            return w
        }
        height: width / aspectRatio
        
        // Preview frame
        Rectangle {
            anchors.fill: parent
            color: "black"
            border.color: Theme.border
            border.width: 1
            
            Image {
                id: previewImage
                anchors.fill: parent
                anchors.margins: 1
                fillMode: Image.PreserveAspectFit
                cache: false
                
                // Use image provider
                source: "image://preview/frame"
                
                // Refresh when frame changes
                Connections {
                    target: PreviewController
                    function onFrameChanged() {
                        previewImage.source = ""
                        previewImage.source = "image://preview/frame?" + Date.now()
                    }
                }
            }
            
            // No content placeholder
            Column {
                anchors.centerIn: parent
                visible: !previewImage.status === Image.Ready || 
                         ProjectController.mediaItems.length === 0
                spacing: Theme.spacingSm
                
                Text {
                    text: "🎬"
                    font.pixelSize: 48
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: qsTr("No video to display")
                    font.pixelSize: Theme.fontSizeMd
                    color: Theme.textDim
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                
                Text {
                    text: qsTr("Import media and add clips to timeline")
                    font.pixelSize: Theme.fontSizeSm
                    color: Theme.textDim
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }
        }
        
        // Resolution overlay
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.spacingSm
            width: resText.width + Theme.spacingXs * 2
            height: resText.height + Theme.spacingXs * 2
            color: Theme.bg1
            opacity: 0.8
            radius: Theme.radiusSm
            
            Text {
                id: resText
                anchors.centerIn: parent
                text: `${PreviewController.previewWidth}×${PreviewController.previewHeight}`
                font.pixelSize: Theme.fontSizeXs
                font.family: Theme.monoFont
                color: Theme.textDim
            }
        }
    }
    
    // Playback indicator
    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Theme.spacingMd
        width: 12
        height: 12
        radius: 6
        color: PreviewController.isPlaying ? Theme.success : 
               PreviewController.isPaused ? Theme.warning : Theme.textDim
        
        SequentialAnimation on opacity {
            running: PreviewController.isPlaying
            loops: Animation.Infinite
            NumberAnimation { to: 0.5; duration: 500 }
            NumberAnimation { to: 1.0; duration: 500 }
        }
    }
}
