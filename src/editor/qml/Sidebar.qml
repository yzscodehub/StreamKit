import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import Phoenix 1.0

/**
 * Right sidebar with properties and media browser
 */
Rectangle {
    id: root
    
    property VideoController controller
    
    color: "#16213e"
    
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        // Tab bar
        TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            TabButton {
                text: qsTr("Properties")
            }
            TabButton {
                text: qsTr("Media")
            }
            TabButton {
                text: qsTr("Effects")
            }
        }
        
        // Tab content
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // Properties tab
            ScrollView {
                clip: true
                
                ColumnLayout {
                    width: parent.width
                    spacing: 16
                    padding: 16
                    
                    GroupBox {
                        title: qsTr("Video Info")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            
                            Label { text: qsTr("File:"); opacity: 0.7 }
                            Label { 
                                text: controller?.source ? 
                                      controller.source.toString().split('/').pop() : "-"
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }
                            
                            Label { text: qsTr("Duration:"); opacity: 0.7 }
                            Label { 
                                text: controller ? formatTime(controller.duration) : "-"
                            }
                            
                            Label { text: qsTr("Resolution:"); opacity: 0.7 }
                            Label { text: "3840 × 2160" }  // TODO: Get from video
                            
                            Label { text: qsTr("Frame Rate:"); opacity: 0.7 }
                            Label { text: "29.97 fps" }  // TODO: Get from video
                            
                            Label { text: qsTr("Codec:"); opacity: 0.7 }
                            Label { text: "H.264" }  // TODO: Get from video
                        }
                    }
                    
                    GroupBox {
                        title: qsTr("Transform")
                        Layout.fillWidth: true
                        
                        GridLayout {
                            columns: 2
                            columnSpacing: 12
                            rowSpacing: 8
                            
                            Label { text: qsTr("Scale:"); opacity: 0.7 }
                            SpinBox {
                                from: 10
                                to: 400
                                value: 100
                                stepSize: 10
                                editable: true
                                
                                textFromValue: function(value) {
                                    return value + "%"
                                }
                            }
                            
                            Label { text: qsTr("Rotation:"); opacity: 0.7 }
                            SpinBox {
                                from: -360
                                to: 360
                                value: 0
                                stepSize: 1
                                editable: true
                                
                                textFromValue: function(value) {
                                    return value + "°"
                                }
                            }
                            
                            Label { text: qsTr("Opacity:"); opacity: 0.7 }
                            Slider {
                                from: 0
                                to: 100
                                value: 100
                                Layout.fillWidth: true
                            }
                        }
                    }
                    
                    Item { Layout.fillHeight: true }
                }
            }
            
            // Media browser tab
            Rectangle {
                color: "transparent"
                
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Media Browser\n(Coming Soon)")
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.5
                }
            }
            
            // Effects tab
            Rectangle {
                color: "transparent"
                
                Label {
                    anchors.centerIn: parent
                    text: qsTr("Effects Library\n(Coming Soon)")
                    horizontalAlignment: Text.AlignHCenter
                    opacity: 0.5
                }
            }
        }
    }
    
    function formatTime(ms) {
        var totalSeconds = Math.floor(ms / 1000)
        var minutes = Math.floor(totalSeconds / 60)
        var seconds = totalSeconds % 60
        return minutes.toString().padStart(2, '0') + ':' + seconds.toString().padStart(2, '0')
    }
}


