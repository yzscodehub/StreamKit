import QtQuick
import QtQuick.Controls

/**
 * Single timeline track containing clips
 */
Rectangle {
    id: root
    
    property var trackData
    property real pixelsPerSecond: 100
    
    color: trackData.type === "video" ? Theme.videoTrack : Theme.audioTrack
    
    // Track content
    Item {
        anchors.fill: parent
        anchors.topMargin: 2
        anchors.bottomMargin: 2
        
        // Clips
        Repeater {
            model: trackData.clips || []
            
            delegate: TimelineClip {
                clipData: modelData
                trackData: root.trackData
                pixelsPerSecond: root.pixelsPerSecond
            }
        }
    }
    
    // Bottom border
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }
    
    // Muted/hidden overlay
    Rectangle {
        anchors.fill: parent
        color: "black"
        opacity: (trackData.muted || trackData.hidden) ? 0.3 : 0
        visible: opacity > 0
        
        Behavior on opacity { NumberAnimation { duration: Theme.animFast } }
    }
    
    // Locked overlay
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        visible: trackData.locked
        
        // Diagonal stripes pattern
        Canvas {
            anchors.fill: parent
            opacity: 0.1
            
            onPaint: {
                var ctx = getContext("2d")
                ctx.strokeStyle = Theme.textDim
                ctx.lineWidth = 1
                
                for (var i = -height; i < width; i += 20) {
                    ctx.beginPath()
                    ctx.moveTo(i, 0)
                    ctx.lineTo(i + height, height)
                    ctx.stroke()
                }
            }
        }
    }
}
