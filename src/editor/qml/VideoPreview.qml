import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import Phoenix 1.0

/**
 * Video preview area with playback display
 */
Rectangle {
    id: root
    
    property VideoController controller
    
    color: "#0f0f1a"
    
    // Video display area
    Rectangle {
        id: videoArea
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, (parent.height - 40) * 16 / 9)
        height: width * 9 / 16
        color: "#000000"
        
        // Placeholder when no video
        Column {
            anchors.centerIn: parent
            spacing: 16
            visible: !controller || controller.source === ""
            
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "🎬"
                font.pixelSize: 64
                opacity: 0.3
            }
            
            Label {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Drop video here or use File → Open")
                font.pixelSize: 16
                opacity: 0.5
                color: "#ffffff"
            }
        }
        
        // TODO: Actual video render surface
        // This will be replaced with OpenGL/Vulkan rendering
        // from PhoenixEngine
        
        // Video info overlay (top-left)
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            visible: controller && controller.source !== ""
            color: Qt.rgba(0, 0, 0, 0.6)
            radius: 4
            width: infoLabel.width + 16
            height: infoLabel.height + 8
            
            Label {
                id: infoLabel
                anchors.centerIn: parent
                text: controller ? controller.source.toString().split('/').pop() : ""
                font.pixelSize: 12
                color: "#ffffff"
            }
        }
    }
    
    // Drop area for video files
    DropArea {
        anchors.fill: parent
        onDropped: (drop) => {
            if (drop.hasUrls) {
                controller.source = drop.urls[0]
            }
        }
    }
    
    // Click to play/pause
    MouseArea {
        anchors.fill: parent
        onClicked: {
            if (controller && controller.source !== "") {
                controller.togglePlayPause()
            }
        }
        onDoubleClicked: {
            // TODO: Toggle fullscreen
        }
    }
}


