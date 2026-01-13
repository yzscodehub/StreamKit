import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Timeline panel - tracks and clips
 */
Rectangle {
    id: root
    color: Theme.bg2
    
    // Top border
    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
        z: 10
    }
    
    RowLayout {
        anchors.fill: parent
        spacing: 0
        
        // Track headers
        ColumnLayout {
            Layout.preferredWidth: Theme.trackHeaderWidth
            Layout.fillHeight: true
            spacing: 0
            
            // Time ruler header
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.timeRulerHeight
                color: Theme.bg3
                
                Text {
                    anchors.centerIn: parent
                    text: qsTr("Tracks")
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Theme.fontWeightMedium
                    color: Theme.textSecondary
                }
                
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                }
            }
            
            // Track header list
            ListView {
                id: trackHeaderList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                interactive: false
                
                model: {
                    let tracks = []
                    for (let t of TimelineController.videoTracks) tracks.push(t)
                    for (let t of TimelineController.audioTracks) tracks.push(t)
                    return tracks
                }
                
                delegate: Rectangle {
                    width: trackHeaderList.width
                    height: Theme.trackHeight
                    color: modelData.type === "video" ? Theme.videoTrack : Theme.audioTrack
                    
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.spacingSm
                        spacing: Theme.spacingXs
                        
                        // Track icon
                        Text {
                            text: modelData.type === "video" ? "🎬" : "🔊"
                            font.pixelSize: Theme.fontSizeMd
                        }
                        
                        // Track name
                        Text {
                            text: modelData.name
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: Theme.fontWeightMedium
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        
                        // Mute button
                        ToolButton {
                            text: modelData.muted ? "🔇" : "🔊"
                            width: 24
                            height: 24
                            font.pixelSize: 12
                            onClicked: TimelineController.setTrackMuted(modelData.id, !modelData.muted)
                            
                            background: Rectangle {
                                color: modelData.muted ? Theme.error + "40" : "transparent"
                                radius: Theme.radiusSm
                            }
                        }
                        
                        // Hide button (video only)
                        ToolButton {
                            visible: modelData.type === "video"
                            text: modelData.hidden ? "👁️‍🗨️" : "👁️"
                            width: 24
                            height: 24
                            font.pixelSize: 12
                            onClicked: TimelineController.setTrackHidden(modelData.id, !modelData.hidden)
                            
                            background: Rectangle {
                                color: modelData.hidden ? Theme.warning + "40" : "transparent"
                                radius: Theme.radiusSm
                            }
                        }
                        
                        // Lock button
                        ToolButton {
                            text: modelData.locked ? "🔒" : "🔓"
                            width: 24
                            height: 24
                            font.pixelSize: 12
                            onClicked: TimelineController.setTrackLocked(modelData.id, !modelData.locked)
                            
                            background: Rectangle {
                                color: modelData.locked ? Theme.info + "40" : "transparent"
                                radius: Theme.radiusSm
                            }
                        }
                    }
                    
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.border
                    }
                }
            }
            
            // Right border
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Theme.border
            }
        }
        
        // Timeline content area
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            // Time ruler
            Rectangle {
                id: timeRuler
                width: parent.width
                height: Theme.timeRulerHeight
                color: Theme.bg3
                clip: true
                z: 5
                
                // Ruler marks
                Repeater {
                    model: Math.ceil(timelineContent.contentWidth / 100) + 1
                    
                    Item {
                        x: index * 100 - timelineContent.contentX
                        width: 100
                        height: parent.height
                        
                        Rectangle {
                            x: 0
                            y: parent.height - 8
                            width: 1
                            height: 8
                            color: Theme.textDim
                        }
                        
                        Text {
                            x: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: TimelineController.formatTime(
                                TimelineController.pixelsToTime(index * 100))
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.monoFont
                            color: Theme.textDim
                        }
                    }
                }
                
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                }
                
                // Ruler click to seek
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        let time = TimelineController.pixelsToTime(
                            mouse.x + timelineContent.contentX)
                        TimelineController.playheadPosition = time
                    }
                }
            }
            
            // Scrollable track content
            Flickable {
                id: timelineContent
                anchors.top: timeRuler.bottom
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                
                contentWidth: TimelineController.timeToPixels(
                    Math.max(TimelineController.duration, 60000000)) // Min 60 seconds
                contentHeight: tracksList.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                
                // Tracks
                Column {
                    id: tracksList
                    
                    Repeater {
                        model: {
                            let tracks = []
                            for (let t of TimelineController.videoTracks) tracks.push(t)
                            for (let t of TimelineController.audioTracks) tracks.push(t)
                            return tracks
                        }
                        
                        delegate: TimelineTrack {
                            width: timelineContent.contentWidth
                            height: Theme.trackHeight
                            trackData: modelData
                            pixelsPerSecond: TimelineController.pixelsPerSecond
                        }
                    }
                }
                
                // Playhead
                Rectangle {
                    id: playhead
                    x: TimelineController.timeToPixels(TimelineController.playheadPosition)
                    y: 0
                    width: 2
                    height: tracksList.height
                    color: Theme.playhead
                    z: 100
                    
                    // Playhead handle
                    Rectangle {
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: -Theme.timeRulerHeight
                        width: 12
                        height: Theme.timeRulerHeight
                        color: Theme.playhead
                        
                        // Triangle
                        Canvas {
                            anchors.bottom: parent.bottom
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 12
                            height: 8
                            
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = Theme.playhead
                                ctx.beginPath()
                                ctx.moveTo(0, 0)
                                ctx.lineTo(12, 0)
                                ctx.lineTo(6, 8)
                                ctx.closePath()
                                ctx.fill()
                            }
                        }
                    }
                    
                    // Glow effect
                    Rectangle {
                        anchors.centerIn: parent
                        width: 8
                        height: parent.height
                        color: Theme.playheadGlow
                        z: -1
                    }
                }
                
                // Click to position playhead
                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onClicked: {
                        let time = TimelineController.pixelsToTime(mouse.x)
                        TimelineController.playheadPosition = time
                    }
                }
                
                // Drop area for media
                DropArea {
                    anchors.fill: parent
                    keys: ["media"]
                    
                    onDropped: {
                        if (drop.hasText || drop.keys.includes("media")) {
                            // Find which track
                            let y = drop.y
                            let tracks = TimelineController.videoTracks
                            let trackIndex = Math.floor(y / Theme.trackHeight)
                            
                            if (trackIndex >= 0 && trackIndex < tracks.length) {
                                let track = tracks[trackIndex]
                                let time = TimelineController.pixelsToTime(drop.x)
                                TimelineController.addClip(
                                    drop.getDataAsString("mediaId"),
                                    track.id,
                                    time)
                            }
                        }
                    }
                }
            }
            
            // Playhead in ruler (fixed position)
            Rectangle {
                x: TimelineController.timeToPixels(TimelineController.playheadPosition) - 
                   timelineContent.contentX
                y: 0
                width: 2
                height: Theme.timeRulerHeight
                color: Theme.playhead
                z: 10
            }
        }
    }
}
