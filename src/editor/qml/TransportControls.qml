import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Phoenix 1.0

/**
 * Playback transport controls (play, pause, seek, etc.)
 */
Rectangle {
    id: root
    
    property VideoController controller
    
    color: "#1a1a2e"
    
    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 16
        
        // Time display (current / total)
        Label {
            id: timeLabel
            text: formatTime(controller ? controller.position : 0) + 
                  " / " + 
                  formatTime(controller ? controller.duration : 0)
            font.pixelSize: 14
            font.family: "Consolas"
            color: "#ffffff"
            Layout.preferredWidth: 120
        }
        
        // Transport buttons
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            
            // Previous frame
            ToolButton {
                icon.source: "qrc:/icons/skip-back.svg"
                icon.width: 20
                icon.height: 20
                onClicked: controller?.seek(0)
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Go to Start")
            }
            
            // Step back
            ToolButton {
                text: "◀◀"
                font.pixelSize: 16
                onClicked: controller?.seek(Math.max(0, controller.position - 5000))
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Back 5s")
            }
            
            // Play/Pause
            RoundButton {
                id: playPauseButton
                width: 48
                height: 48
                Material.background: Material.accent
                
                text: (!controller?.playing || controller?.paused) ? "▶" : "⏸"
                font.pixelSize: 20
                
                onClicked: controller?.togglePlayPause()
                
                ToolTip.visible: hovered
                ToolTip.text: controller?.playing && !controller?.paused ? qsTr("Pause") : qsTr("Play")
            }
            
            // Step forward
            ToolButton {
                text: "▶▶"
                font.pixelSize: 16
                onClicked: controller?.seek(Math.min(controller.duration, controller.position + 5000))
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Forward 5s")
            }
            
            // Stop
            ToolButton {
                text: "⏹"
                font.pixelSize: 20
                onClicked: controller?.stop()
                
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Stop")
            }
        }
        
        // Seek slider
        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0
            to: controller ? controller.duration : 1000
            value: controller ? controller.position : 0
            
            onMoved: {
                controller?.seek(value)
            }
            
            Material.accent: Material.Teal
        }
        
        // Volume control
        RowLayout {
            spacing: 4
            
            Label {
                text: "🔊"
                font.pixelSize: 16
            }
            
            Slider {
                id: volumeSlider
                Layout.preferredWidth: 80
                from: 0
                to: 1
                value: controller ? controller.volume : 1
                
                onMoved: {
                    if (controller) controller.volume = value
                }
                
                Material.accent: Material.Teal
            }
        }
    }
    
    function formatTime(ms) {
        var totalSeconds = Math.floor(ms / 1000)
        var hours = Math.floor(totalSeconds / 3600)
        var minutes = Math.floor((totalSeconds % 3600) / 60)
        var seconds = totalSeconds % 60
        
        if (hours > 0) {
            return hours.toString() + ':' + 
                   minutes.toString().padStart(2, '0') + ':' + 
                   seconds.toString().padStart(2, '0')
        } else {
            return minutes.toString().padStart(2, '0') + ':' + 
                   seconds.toString().padStart(2, '0')
        }
    }
}


