import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/**
 * Property panel - shows properties of selected item
 */
Rectangle {
    id: root
    color: Theme.bg2
    
    // Left border
    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.border
    }
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 36
            color: Theme.bg3
            
            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.spacingMd
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Properties")
                font.pixelSize: Theme.fontSizeMd
                font.weight: Theme.fontWeightMedium
                color: Theme.textPrimary
            }
            
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }
        
        // Content
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            ColumnLayout {
                width: parent.width
                spacing: Theme.spacingMd
                
                Item { height: Theme.spacingSm }
                
                // Show clip properties if selected
                Loader {
                    Layout.fillWidth: true
                    Layout.margins: Theme.spacingMd
                    active: TimelineController.selectedClipId !== ""
                    
                    sourceComponent: ColumnLayout {
                        spacing: Theme.spacingMd
                        
                        // Clip section
                        PropertySection {
                            title: qsTr("Clip")
                            
                            PropertyRow {
                                label: qsTr("Name")
                                value: {
                                    let clips = []
                                    for (let t of TimelineController.videoTracks) {
                                        for (let c of t.clips) clips.push(c)
                                    }
                                    let clip = clips.find(c => c.id === TimelineController.selectedClipId)
                                    return clip ? clip.name : ""
                                }
                            }
                            
                            PropertyRow {
                                label: qsTr("Duration")
                                value: {
                                    let clips = []
                                    for (let t of TimelineController.videoTracks) {
                                        for (let c of t.clips) clips.push(c)
                                    }
                                    let clip = clips.find(c => c.id === TimelineController.selectedClipId)
                                    return clip ? TimelineController.formatTime(clip.duration) : ""
                                }
                            }
                            
                            PropertyRow {
                                label: qsTr("Position")
                                value: {
                                    let clips = []
                                    for (let t of TimelineController.videoTracks) {
                                        for (let c of t.clips) clips.push(c)
                                    }
                                    let clip = clips.find(c => c.id === TimelineController.selectedClipId)
                                    return clip ? TimelineController.formatTime(clip.timelineIn) : ""
                                }
                            }
                        }
                    }
                }
                
                // Show project settings if no clip selected
                Loader {
                    Layout.fillWidth: true
                    Layout.margins: Theme.spacingMd
                    active: TimelineController.selectedClipId === ""
                    
                    sourceComponent: ColumnLayout {
                        spacing: Theme.spacingMd
                        
                        PropertySection {
                            title: qsTr("Sequence")
                            
                            PropertyRow {
                                label: qsTr("Resolution")
                                value: `${ProjectController.frameWidth}×${ProjectController.frameHeight}`
                            }
                            
                            PropertyRow {
                                label: qsTr("Frame Rate")
                                value: `${ProjectController.frameRate.toFixed(2)} fps`
                            }
                            
                            PropertyRow {
                                label: qsTr("Duration")
                                value: TimelineController.formatTime(TimelineController.duration)
                            }
                            
                            PropertyRow {
                                label: qsTr("Video Tracks")
                                value: TimelineController.videoTrackCount.toString()
                            }
                            
                            PropertyRow {
                                label: qsTr("Audio Tracks")
                                value: TimelineController.audioTrackCount.toString()
                            }
                        }
                    }
                }
                
                Item { Layout.fillHeight: true }
            }
        }
    }
    
    // Property section component
    component PropertySection: ColumnLayout {
        property string title: ""
        default property alias content: contentColumn.children
        
        Layout.fillWidth: true
        spacing: Theme.spacingSm
        
        Text {
            text: title
            font.pixelSize: Theme.fontSizeSm
            font.weight: Theme.fontWeightMedium
            color: Theme.textAccent
        }
        
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }
        
        ColumnLayout {
            id: contentColumn
            Layout.fillWidth: true
            spacing: Theme.spacingXs
        }
    }
    
    // Property row component
    component PropertyRow: RowLayout {
        property string label: ""
        property string value: ""
        
        Layout.fillWidth: true
        
        Text {
            text: label + ":"
            font.pixelSize: Theme.fontSizeSm
            color: Theme.textSecondary
            Layout.preferredWidth: 80
        }
        
        Text {
            text: value
            font.pixelSize: Theme.fontSizeSm
            font.family: Theme.monoFont
            color: Theme.textPrimary
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }
}
