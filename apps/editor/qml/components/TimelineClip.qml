import QtQuick
import QtQuick.Controls

/**
 * Single clip on the timeline
 */
Rectangle {
    id: root
    
    property var clipData
    property var trackData
    property real pixelsPerSecond: 100
    
    x: TimelineController.timeToPixels(clipData.timelineIn)
    width: TimelineController.timeToPixels(clipData.duration)
    height: parent.height
    
    color: clipData.selected ? 
           (trackData.type === "video" ? Theme.clipVideoSelected : Theme.clipAudioSelected) :
           (trackData.type === "video" ? Theme.clipVideo : Theme.clipAudio)
    
    radius: Theme.radiusSm
    border.color: clipData.selected ? "white" : Qt.darker(color, 1.2)
    border.width: clipData.selected ? 2 : 1
    
    // Clip content
    Item {
        anchors.fill: parent
        anchors.margins: 4
        clip: true
        
        // Clip name
        Text {
            anchors.left: parent.left
            anchors.top: parent.top
            text: clipData.name
            font.pixelSize: Theme.fontSizeXs
            font.weight: Theme.fontWeightMedium
            color: "white"
            elide: Text.ElideRight
            width: parent.width
        }
        
        // Duration
        Text {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            text: TimelineController.formatTime(clipData.duration)
            font.pixelSize: Theme.fontSizeXs
            font.family: Theme.monoFont
            color: Qt.rgba(1, 1, 1, 0.7)
            visible: root.width > 60
        }
        
        // Waveform placeholder (for audio)
        Canvas {
            anchors.fill: parent
            anchors.topMargin: 16
            visible: trackData.type === "audio"
            opacity: 0.5
            
            onPaint: {
                var ctx = getContext("2d")
                ctx.strokeStyle = "white"
                ctx.lineWidth = 1
                
                var h = height / 2
                ctx.beginPath()
                ctx.moveTo(0, h)
                
                for (var x = 0; x < width; x += 3) {
                    var amp = Math.random() * h * 0.8
                    ctx.lineTo(x, h - amp)
                    ctx.lineTo(x + 1, h + amp)
                }
                
                ctx.stroke()
            }
        }
    }
    
    // Trim handles
    Rectangle {
        id: leftHandle
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        color: leftHandleArea.containsMouse ? Theme.accent : "transparent"
        radius: Theme.radiusSm
        
        MouseArea {
            id: leftHandleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            
            property real startX: 0
            property real startIn: 0
            
            onPressed: {
                startX = mouseX
                startIn = clipData.timelineIn
            }
            
            onPositionChanged: {
                if (pressed) {
                    let delta = mouseX - startX
                    let newIn = startIn + TimelineController.pixelsToTime(delta)
                    TimelineController.trimClipStart(clipData.id, Math.max(0, newIn))
                }
            }
        }
    }
    
    Rectangle {
        id: rightHandle
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
        color: rightHandleArea.containsMouse ? Theme.accent : "transparent"
        radius: Theme.radiusSm
        
        MouseArea {
            id: rightHandleArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.SizeHorCursor
            
            property real startX: 0
            property real startOut: 0
            
            onPressed: {
                startX = mouseX
                startOut = clipData.timelineOut
            }
            
            onPositionChanged: {
                if (pressed) {
                    let delta = mouseX - startX
                    let newOut = startOut + TimelineController.pixelsToTime(delta)
                    TimelineController.trimClipEnd(clipData.id, newOut)
                }
            }
        }
    }
    
    // Main drag area
    MouseArea {
        id: dragArea
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        hoverEnabled: true
        cursorShape: trackData.locked ? Qt.ForbiddenCursor : Qt.OpenHandCursor
        
        property real startX: 0
        property real startClipX: 0
        
        onPressed: {
            if (!trackData.locked) {
                TimelineController.selectClip(clipData.id)
                startX = mouse.x
                startClipX = root.x
                cursorShape = Qt.ClosedHandCursor
            }
        }
        
        onReleased: {
            cursorShape = Qt.OpenHandCursor
        }
        
        onPositionChanged: {
            if (pressed && !trackData.locked) {
                let delta = mouse.x - startX
                let newX = startClipX + delta
                let newTime = TimelineController.pixelsToTime(newX)
                TimelineController.moveClip(clipData.id, trackData.id, Math.max(0, newTime))
            }
        }
        
        onDoubleClicked: {
            // Could open clip properties
        }
    }
    
    // Context menu
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.RightButton
        
        onClicked: {
            TimelineController.selectClip(clipData.id)
            contextMenu.popup()
        }
    }
    
    Menu {
        id: contextMenu
        
        Action {
            text: qsTr("Split at Playhead")
            onTriggered: TimelineController.splitClipAtPlayhead(clipData.id)
        }
        
        MenuSeparator {}
        
        Action {
            text: qsTr("Delete")
            onTriggered: TimelineController.deleteClip(clipData.id)
        }
    }
    
    // Selection animation
    Behavior on border.width { NumberAnimation { duration: Theme.animFast } }
    Behavior on color { ColorAnimation { duration: Theme.animFast } }
}
