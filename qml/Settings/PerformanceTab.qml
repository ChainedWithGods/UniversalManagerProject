import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ColumnLayout {
    Layout.fillWidth: true
    Layout.rightMargin: 10
    spacing: 16
    visible: generalSectionIndex === 7

    component UiverseToggle : Item {
        id: uiverseToggle
        property bool checked: false
        signal toggled(bool checked)

        width: 46
        height: 24

        readonly property real _circleDiameter: 18
        readonly property real _offset: (height - _circleDiameter) / 2
        readonly property color _accent: window.primaryColor
        readonly property color _offBg: Qt.rgba(_accent.r, _accent.g, _accent.b, modManager.theme === "Cozy" ? 0.35 : 0.25)

        Rectangle {
            id: slider
            anchors.fill: parent
            radius: height / 2
            color: uiverseToggle.checked ? uiverseToggle._accent : uiverseToggle._offBg

            Behavior on color { ColorAnimation { duration: 200; easing.type: Easing.OutBack } }

            Rectangle {
                width: uiverseToggle._circleDiameter / 2
                height: width / 2 - 1
                radius: 1
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width - (width / 2))
                   : (uiverseToggle._offset + (width / 2))

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
            }

            Rectangle {
                id: circle
                width: uiverseToggle._circleDiameter
                height: uiverseToggle._circleDiameter
                radius: width / 2
                color: "#ffffff"
                anchors.verticalCenter: parent.verticalCenter
                x: uiverseToggle.checked
                   ? (parent.width - uiverseToggle._offset - width)
                   : uiverseToggle._offset

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: Qt.rgba(0, 0, 0, 0.32)
                    shadowHorizontalOffset: 1
                    shadowVerticalOffset: 1
                    shadowBlur: 0.45
                }

                Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

                Text {
                    anchors.centerIn: parent
                    text: "×"
                    font.pixelSize: 14
                    font.weight: Font.Bold
                    color: Qt.rgba(uiverseToggle._accent.r, uiverseToggle._accent.g, uiverseToggle._accent.b, 1.0)
                    scale: uiverseToggle.checked ? 0 : 1
                    opacity: uiverseToggle.checked ? 0 : 1
                    Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }
                    Behavior on opacity { NumberAnimation { duration: 120; easing.type: Easing.InOutQuad } }
                }
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: uiverseToggle.toggled(!uiverseToggle.checked)
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 2

        Text {
            text: "Performance"
            font.pixelSize: 22
            font.weight: Font.Bold
            color: window.textColor
        }

        Text {
            text: "Monitor system resources and optimize application behavior."
            font.pixelSize: 12
            color: window.textSecondaryColor
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 16

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: 580
            implicitHeight: 215
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: 12
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonRamCleanerGradient : null
            Gradient {
                id: crimsonRamCleanerGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.12)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 3
                shadowBlur: 0.4
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "Aggressive RAM Cleaner"
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Rectangle {
                        implicitWidth: autoBadge.width + 10
                        height: 18
                        radius: 5
                        color: aggressiveRam.enabled ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15) : Qt.rgba(107, 114, 128, 0.15)

                        Text {
                            id: autoBadge
                            anchors.centerIn: parent
                            text: "AUTO"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                            color: aggressiveRam.enabled ? window.accentColor : "#6B7280"
                        }
                    }

                    Item { Layout.fillWidth: true }

                    UiverseToggle {
                        checked: aggressiveRam.enabled
                        onToggled: (value) => aggressiveRam.enabled = value
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Rectangle {
                        Layout.fillWidth: true
                        height: 54
                        radius: 8
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                text: "Current Usage"
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: aggressiveRam.currentRamUsageMB + " MB"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: aggressiveRam.currentRamUsageMB >= aggressiveRam.ramThresholdMB ? "#EF4444" : window.primaryColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 54
                        radius: 8
                        color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.12)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                text: "Threshold"
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: aggressiveRam.ramThresholdMB + " MB"
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: window.accentColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 54
                        radius: 8
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.08)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 1

                            Text {
                                text: "Cleanups"
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                            Text {
                                text: aggressiveRam.cleanupCount
                                font.pixelSize: 16
                                font.weight: Font.Bold
                                color: window.primaryColor
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 14

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "RAM Threshold Target"
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: aggressiveRam.ramThresholdMB + " MB"
                                font.pixelSize: 12
                                font.weight: Font.Bold
                                color: window.accentColor
                            }
                        }

                        Slider {
                            Layout.fillWidth: true
                            implicitHeight: 22
                            from: 100
                            to: 2000
                            stepSize: 50
                            value: aggressiveRam.ramThresholdMB
                            onMoved: aggressiveRam.ramThresholdMB = value

                            background: Rectangle {
                                x: parent.leftPadding
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 200
                                implicitHeight: 5
                                width: parent.availableWidth
                                height: implicitHeight
                                radius: 2.5
                                color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.15)

                                Rectangle {
                                    width: parent.parent.visualPosition * parent.width
                                    height: parent.height
                                    gradient: Gradient {
                                        orientation: Gradient.Horizontal
                                        GradientStop { position: 0.0; color: window.primaryColor }
                                        GradientStop { position: 1.0; color: window.accentColor }
                                    }
                                    radius: 2.5
                                }
                            }

                            handle: Rectangle {
                                x: parent.leftPadding + parent.visualPosition * (parent.availableWidth - width)
                                y: parent.topPadding + parent.availableHeight / 2 - height / 2
                                implicitWidth: 16
                                implicitHeight: 16
                                radius: 8
                                color: "#FFFFFF"
                                border.color: window.accentColor
                                border.width: 2.5
                            }
                        }
                    }

                    Button {
                        Layout.preferredWidth: 90
                        Layout.preferredHeight: 36
                        Layout.alignment: Qt.AlignVCenter

                        background: Rectangle {
                            radius: 8
                            color: parent.down ? Qt.darker(window.accentColor, 1.15) : (parent.hovered ? Qt.lighter(window.accentColor, 1.1) : window.accentColor)
                            Behavior on color { ColorAnimation { duration: 150 } }
                        }

                        contentItem: Text {
                            text: "Clean"
                            font.pixelSize: 13
                            font.weight: Font.Bold
                            color: "#FFFFFF"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: aggressiveRam.performCleanup()
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredWidth: 380
            implicitHeight: 215
            color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
            radius: 12
            border.width: window.borderWidth
            border.color: window.borderColor

            gradient: modManager.theme === "Crimson" ? crimsonBehaviorCardGradient : null
            Gradient {
                id: crimsonBehaviorCardGradient
                GradientStop { position: 0.0; color: window.crimsonGradientTop }
                GradientStop { position: 1.0; color: window.crimsonGradientBottom }
            }

            layer.enabled: modManager.theme !== "Glassmorphic"
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.12)
                shadowHorizontalOffset: 0
                shadowVerticalOffset: 3
                shadowBlur: 0.4
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 12

                Text {
                    text: "Performance Behavior"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: window.textColor
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        Text {
                            text: "Pause when app loses focus"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: window.textColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: "Disable animations when switching windows."
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    UiverseToggle {
                        checked: modManager.pauseOnDefocus
                        onToggled: (value) => modManager.setPauseOnDefocus(value)
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.1)
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2

                        RowLayout {
                            spacing: 6
                            Text {
                                text: "Optimized Mode"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                color: window.textColor
                            }
                            Rectangle {
                                implicitWidth: betaBadge.width + 8
                                height: 16
                                radius: 4
                                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.15)
                                Text {
                                    id: betaBadge
                                    anchors.centerIn: parent
                                    text: "BETA"
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    color: window.accentColor
                                }
                            }
                        }

                        Text {
                            text: "Reduce memory by 90% when unfocused."
                            font.pixelSize: 12
                            color: window.textSecondaryColor
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    UiverseToggle {
                        checked: modManager.pauseStatsOnDefocus
                        onToggled: (value) => modManager.setPauseStatsOnDefocus(value)
                    }
                }
            }
        }
    }

    ColumnLayout {
        Layout.fillWidth: true
        spacing: 10

        Text {
            text: "System Overview"
            font.pixelSize: 15
            font.weight: Font.DemiBold
            color: window.textColor
        }

        Flow {
            id: hardwareFlow
            Layout.fillWidth: true
            spacing: 12

            readonly property real itemWidth: Math.max(200, Math.floor((width - spacing * 3) / 4))

            Rectangle {
                width: hardwareFlow.itemWidth
                height: 120
                color: modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor
                radius: 10
                border.width: window.borderWidth
                border.color: window.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    Text {
                        text: "CPU Usage"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: window.textSecondaryColor
                        opacity: 0.8
                    }

                    Text {
                        text: modManager ? modManager.cpuUsage.toFixed(1) + "%" : "0%"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: Qt.rgba(0,0,0,0.08)

                        Rectangle {
                            width: Math.round(parent.width * (modManager ? modManager.cpuUsage / 100 : 0))
                            height: parent.height
                            radius: 3
                            property color barColor: modManager ? modManager.cpuUsageBarColor : "#10B981"
                            color: barColor
                            Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 400 } }
                        }
                    }
                }
            }

            Rectangle {
                width: hardwareFlow.itemWidth
                height: 120
                color: modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor
                radius: 10
                border.width: window.borderWidth
                border.color: window.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    Text {
                        text: "RAM Usage"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: window.textSecondaryColor
                        opacity: 0.8
                    }

                    Text {
                        text: modManager ? modManager.ramUsageText : "0 / 0 GB"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        color: window.textColor
                    }

                    Item { Layout.fillHeight: true }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 6
                        radius: 3
                        color: Qt.rgba(0,0,0,0.08)

                        Rectangle {
                            width: Math.round(parent.width * (modManager ? modManager.ramUsage / 100 : 0))
                            height: parent.height
                            radius: 3
                            property color barColor: modManager ? modManager.ramUsageBarColor : "#3B82F6"
                            color: barColor
                            Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            Behavior on color { ColorAnimation { duration: 400 } }
                        }
                    }
                }
            }

            Repeater {
                model: modManager ? modManager.driveStats : []
                Rectangle {
                    width: hardwareFlow.itemWidth
                    height: 120
                    color: modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor
                    radius: 10
                    border.width: window.borderWidth
                    border.color: window.borderColor

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Text {
                                text: "Disk " + modelData.name.replace("\\", "").replace("/", "")
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: window.textSecondaryColor
                                opacity: 0.8
                            }

                            Item { Layout.fillWidth: true }

                            Rectangle {
                                visible: modelData.label !== ""
                                implicitWidth: driveLabel.width + 10
                                height: 18
                                radius: 4
                                color: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.1)

                                Text {
                                    id: driveLabel
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.pixelSize: 9
                                    font.weight: Font.Medium
                                    color: window.accentColor
                                }
                            }
                        }

                        Text {
                            text: modelData.usageText
                            font.pixelSize: 18
                            font.weight: Font.Bold
                            color: window.textColor
                        }

                        Item { Layout.fillHeight: true }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: Qt.rgba(0,0,0,0.08)

                            Rectangle {
                                width: Math.round(parent.width * (modelData.usagePercent / 100))
                                height: parent.height
                                radius: 3
                                property color barColor: modManager ? modManager.driveUsageBarColor : "#8B5CF6"
                                color: barColor
                            }
                        }
                    }
                }
            }
        }
    }
}
