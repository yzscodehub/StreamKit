import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Phoenix 1.0

/**
 * Timeline with tracks and clips
 */
Rectangle {
    id: root
    
    property VideoController controller
    
    color: "#16213e"
    
    // Inline component definition (must be inside root)
    component TrackRow: Rectangle {
        property string trackName: "Track"
        property color trackColor: "#2d4059"
        property bool hasClip: false
        property int clipDuration: 0
        
        Layout.fillWidth: true
        Layout.preferredHeight: 50
        color: "#1a1a2e"
        
        RowLayout {
            anchors.fill: parent
            spacing: 0
            
            // Track label
            Rectangle {
                Layout.preferredWidth: 150
                Layout.fillHeight: true
                color: "#16213e"
                border.color: "#3d5a80"
                border.width: 1
                
                Label {
                    anchors.centerIn: parent
                    text: trackName
                    font.pixelSize: 12
                    color: "#ffffff"
                }
            }
            
            // Track content
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"
                
                // Clip rectangle
                Rectangle {
                    visible: hasClip
                    x: 0
                    y: 4
                    width: clipDuration / 10  // 100px per second
                    height: parent.height - 8
                    radius: 4
                    color: trackColor
                    opacity: 0.8
                    
                    Label {
                        anchors.left: parent.left
                        anchors.leftMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Clip"
                        font.pixelSize: 11
                        color: "#ffffff"
                    }
                }
            }
        }
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Timeline header with time ruler
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 30
            color: "#1a1a2e"
            
            // Time ruler
            Row {
                anchors.fill: parent
                anchors.leftMargin: 150  // Track label width
                spacing: 0
                
                Repeater {
                    model: controller ? Math.ceil(controller.duration / 1000) : 30  // Every second
                    
                    Rectangle {
                        width: 100  // 100px per second (adjustable zoom)
                        height: parent.height
                        color: "transparent"
                        
                        Rectangle {
                            anchors.left: parent.left
                            width: 1
                            height: parent.height
                            color: "#3d5a80"
                        }
                        
                        Label {
                            anchors.left: parent.left
                            anchors.leftMargin: 4
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.formatTime(index * 1000)
                            font.pixelSize: 10
                            color: "#7f8c8d"
                        }
                    }
                }
            }
            
            // Playhead position line (extends to tracks)
            Rectangle {
                id: playhead
                x: 150 + (controller ? controller.position / 10 : 0)  // 100px/sec
                width: 2
                height: root.height
                color: Material.accent
                z: 100
                
                // Playhead handle
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    width: 12
                    height: 12
                    radius: 6
                    color: Material.accent
                }
            }
        }
        
        // Tracks area
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            
            ColumnLayout {
                width: parent.width
                spacing: 2
                
                // Video track
                TrackRow {
                    trackName: qsTr("Video 1")
                    trackColor: "#e94560"
                    hasClip: controller && controller.source !== ""
                    clipDuration: controller ? controller.duration : 0
                }
                
                // Audio track
                TrackRow {
                    trackName: qsTr("Audio 1")
                    trackColor: "#0f3460"
                    hasClip: controller && controller.source !== ""
                    clipDuration: controller ? controller.duration : 0
                }
                
                // Empty tracks
                Repeater {
                    model: 3
                    TrackRow {
                        trackName: qsTr("Track %1").arg(index + 3)
                        trackColor: "#2d4059"
                        hasClip: false
                    }
                }
            }
        }
    }
    
    function formatTime(ms) {
        var seconds = Math.floor(ms / 1000)
        var minutes = Math.floor(seconds / 60)
        seconds = seconds % 60
        return minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0')
    }
}
