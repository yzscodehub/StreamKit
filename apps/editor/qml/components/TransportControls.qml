import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Transport controls - playback buttons and time display
 */
Rectangle {
    id: root
    color: Theme.bg3
    
    // Top border
    Rectangle {
        width: parent.width
        height: 1
        color: Theme.border
    }
    
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spacingLg
        anchors.rightMargin: Theme.spacingLg
        spacing: Theme.spacingMd
        
        // Timecode display
        Rectangle {
            Layout.preferredWidth: 140
            Layout.preferredHeight: 32
            color: Theme.bg1
            radius: Theme.radiusSm
            
            Text {
                anchors.centerIn: parent
                text: TimelineController.formatTime(PreviewController.position)
                font.pixelSize: Theme.fontSizeLg
                font.family: Theme.monoFont
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // Transport buttons
        RowLayout {
            spacing: Theme.spacingSm
            
            // Go to start
            TransportButton {
                icon.source: "qrc:/Phoenix/resources/icons/skip-back.svg"
                ToolTip.text: qsTr("Go to Start (Home)")
                onClicked: PreviewController.goToStart()
            }
            
            // Step back
            TransportButton {
                text: "◀"
                ToolTip.text: qsTr("Step Backward (←)")
                onClicked: PreviewController.stepBackward()
            }
            
            // Play/Pause
            TransportButton {
                id: playButton
                width: 48
                height: 48
                icon.source: PreviewController.isPlaying ? 
                    "qrc:/Phoenix/resources/icons/pause.svg" :
                    "qrc:/Phoenix/resources/icons/play.svg"
                icon.width: 24
                icon.height: 24
                ToolTip.text: PreviewController.isPlaying ? 
                    qsTr("Pause (Space)") : qsTr("Play (Space)")
                onClicked: PreviewController.togglePlayPause()
                
                highlighted: true
                
                background: Rectangle {
                    color: playButton.pressed ? Theme.accentDim :
                           playButton.hovered ? Theme.accentHover : Theme.accent
                    radius: width / 2
                }
            }
            
            // Step forward
            TransportButton {
                text: "▶"
                ToolTip.text: qsTr("Step Forward (→)")
                onClicked: PreviewController.stepForward()
            }
            
            // Go to end
            TransportButton {
                icon.source: "qrc:/Phoenix/resources/icons/skip-forward.svg"
                ToolTip.text: qsTr("Go to End (End)")
                onClicked: PreviewController.goToEnd()
            }
            
            // Stop
            TransportButton {
                icon.source: "qrc:/Phoenix/resources/icons/stop.svg"
                ToolTip.text: qsTr("Stop (Esc)")
                onClicked: PreviewController.stop()
            }
        }
        
        Item { Layout.fillWidth: true }
        
        // Playback speed
        RowLayout {
            spacing: Theme.spacingXs
            
            Text {
                text: qsTr("Speed:")
                font.pixelSize: Theme.fontSizeSm
                color: Theme.textSecondary
            }
            
            ComboBox {
                id: speedCombo
                model: ["0.25x", "0.5x", "1x", "1.5x", "2x", "4x"]
                currentIndex: 2
                
                onCurrentIndexChanged: {
                    let speeds = [0.25, 0.5, 1.0, 1.5, 2.0, 4.0]
                    PreviewController.playbackSpeed = speeds[currentIndex]
                }
                
                background: Rectangle {
                    implicitWidth: 70
                    implicitHeight: 28
                    color: Theme.bg4
                    radius: Theme.radiusSm
                    border.color: speedCombo.visualFocus ? Theme.accent : Theme.border
                }
                
                contentItem: Text {
                    text: speedCombo.displayText
                    font.pixelSize: Theme.fontSizeSm
                    font.family: Theme.monoFont
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: Theme.spacingSm
                }
            }
            
            // Loop toggle
            TransportButton {
                checkable: true
                checked: PreviewController.looping
                text: "🔁"
                ToolTip.text: qsTr("Loop Playback")
                onClicked: PreviewController.looping = checked
                
                background: Rectangle {
                    color: parent.checked ? Theme.accentBg :
                           parent.hovered ? Theme.bg5 : "transparent"
                    radius: Theme.radiusSm
                    border.color: parent.checked ? Theme.accent : "transparent"
                }
            }
        }
        
        // Duration display
        Rectangle {
            Layout.preferredWidth: 140
            Layout.preferredHeight: 32
            color: Theme.bg1
            radius: Theme.radiusSm
            
            Text {
                anchors.centerIn: parent
                text: TimelineController.formatTime(PreviewController.duration)
                font.pixelSize: Theme.fontSizeLg
                font.family: Theme.monoFont
                font.weight: Theme.fontWeightMedium
                color: Theme.textDim
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
    
    // Transport button component
    component TransportButton: ToolButton {
        width: 36
        height: 36
        icon.width: Theme.iconSizeMd
        icon.height: Theme.iconSizeMd
        icon.color: Theme.textPrimary
        
        ToolTip.visible: hovered
        ToolTip.delay: 500
        
        background: Rectangle {
            color: parent.pressed ? Theme.bg5 :
                   parent.hovered ? Theme.bg4 : "transparent"
            radius: Theme.radiusSm
        }
        
        contentItem: Item {
            Text {
                anchors.centerIn: parent
                text: parent.parent.text
                font.pixelSize: Theme.fontSizeMd
                color: Theme.textPrimary
                visible: parent.parent.text !== ""
            }
        }
    }
}
