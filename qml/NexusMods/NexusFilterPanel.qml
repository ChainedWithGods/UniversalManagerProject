import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Rectangle {
    property var page: null
    id: filterSidebar

    visible: page.showFilters
    Layout.preferredWidth: 290
    Layout.fillHeight: true
    radius: window.cardRadius
    color: modManager.theme === "Crimson" ? page.deepPanelColor : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
    border.width: window.borderWidth
    border.color: window.borderColor
    clip: true

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        spacing: 14

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Text {
                text: "Filters"
                font.pixelSize: 16
                font.weight: Font.DemiBold
                color: window.textColor
            }

            Rectangle {
                visible: modManager.nexusFilterActiveCount > 0
                Layout.preferredWidth: filterCountLabel.implicitWidth + 12
                Layout.preferredHeight: 18
                radius: 9
                color: window.primaryColor

                Text {
                    id: filterCountLabel
                    anchors.centerIn: parent
                    text: modManager.nexusFilterActiveCount
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                visible: modManager.nexusFilterActiveCount > 0
                text: "Reset All"
                font.pixelSize: 11
                font.weight: Font.DemiBold
                color: window.primaryColor
                opacity: resetAllMouse.containsMouse ? 0.7 : 1
                Behavior on opacity { NumberAnimation { duration: 120 } }

                MouseArea {
                    id: resetAllMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        modManager.resetAllNexusFilters()
                        titleParamInput.text = ""
                        descParamInput.text = ""
                        authorParamInput.text = ""
                        uploaderParamInput.text = ""
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 13
                color: closeFilterMouse.containsMouse
                    ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.1)
                    : "transparent"
                Behavior on color { ColorAnimation { duration: 120 } }

                Image {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    source: "qrc:/UniversalModManager/Lucide Icons/x.svg"
                    sourceSize: Qt.size(13, 13)
                    smooth: true

                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                    }
                }

                MouseArea {
                    id: closeFilterMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: page.showFilters = false
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: window.borderColor
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: filterItemsCol.implicitHeight + 20
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {
                id: filterScrollBar
                policy: ScrollBar.AsNeeded
                width: 8
                padding: 2

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: filterScrollBar.pressed
                        ? window.primaryColor
                        : (filterScrollBar.hovered
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.28))
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            Text {
                visible: !page.hasNexusCredentials
                width: parent.width - 6
                y: 8
                text: "Sign in or add an API key to load the filter options."
                font.pixelSize: 12
                color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.55)
                wrapMode: Text.WordWrap
            }

            ColumnLayout {
                id: filterItemsCol
                visible: page.hasNexusCredentials
                width: parent.width - 6
                spacing: 16

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    NexusFilterSectionLabel {
                        text: "TIME RANGE"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        radius: 8
                        color: timeRangeMouse.containsMouse ? Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.08) : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.04)
                        border.width: 1
                        border.color: timeRangePopup.visible ? window.primaryColor : window.borderColor

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Text {
                                Layout.fillWidth: true
                                text: {
                                    switch (modManager.nexusTimeRange) {
                                        case "24h": return "24 Hours"
                                        case "7d": return "7 Days"
                                        case "14d": return "14 Days"
                                        case "28d": return "28 Days"
                                        case "1y": return "1 Year"
                                        default: return "All time"
                                    }
                                }
                                font.pixelSize: 12
                                font.weight: Font.Medium
                                color: window.textColor
                            }

                            Image {
                                width: 14
                                height: 14
                                source: "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
                                sourceSize: Qt.size(14, 14)
                                smooth: true
                                rotation: timeRangePopup.visible ? 180 : 0
                                Behavior on rotation { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.65)
                                }
                            }
                        }

                        MouseArea {
                            id: timeRangeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: timeRangePopup.open()
                        }

                        Popup {
                            id: timeRangePopup
                            y: parent.height + 4
                            width: parent.width
                            padding: 6
                            background: Rectangle {
                                radius: 8
                                color: page.deepPanelColor
                                border.width: 1
                                border.color: window.borderColor

                                layer.enabled: true
                                layer.effect: MultiEffect {
                                    shadowEnabled: true
                                    shadowColor: Qt.rgba(0, 0, 0, 0.15)
                                    shadowVerticalOffset: 4
                                    shadowBlur: 0.8
                                }
                            }

                            ColumnLayout {
                                width: parent.width
                                spacing: 2

                                Repeater {
                                    model: [
                                        { label: "All time", key: "all" },
                                        { label: "24 Hours", key: "24h" },
                                        { label: "7 Days", key: "7d" },
                                        { label: "14 Days", key: "14d" },
                                        { label: "28 Days", key: "28d" },
                                        { label: "1 Year", key: "1y" }
                                    ]

                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 32
                                        radius: 6
                                        color: timeItemMouse.containsMouse ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.15) : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 10
                                            anchors.rightMargin: 10

                                            Text {
                                                Layout.fillWidth: true
                                                text: modelData.label
                                                font.pixelSize: 12
                                                color: (modManager.nexusTimeRange === modelData.key) ? window.primaryColor : window.textColor
                                                font.weight: (modManager.nexusTimeRange === modelData.key) ? Font.DemiBold : Font.Normal
                                            }

                                            Image {
                                                Layout.alignment: Qt.AlignVCenter
                                                Layout.preferredWidth: 13
                                                Layout.preferredHeight: 13
                                                source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                                sourceSize: Qt.size(13, 13)
                                                smooth: true
                                                visible: modManager.nexusTimeRange === modelData.key

                                                layer.enabled: true
                                                layer.effect: ColorOverlay {
                                                    color: window.primaryColor
                                                }
                                            }
                                        }

                                        MouseArea {
                                            id: timeItemMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                modManager.setNexusTimeRange(modelData.key)
                                                timeRangePopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                NexusFilterDivider { }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "CATEGORY"
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.letterSpacing: 1.2
                            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.55)
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            visible: modManager.nexusSelectedCategories.length > 0
                            text: "Clear"
                            font.pixelSize: 11
                            color: window.primaryColor
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: modManager.clearNexusCategories()
                            }
                        }
                    }

                    Repeater {
                        model: modManager.nexusCategoriesList

                        delegate: NexusFilterCheckBox {
                            label: modelData.name
                            countText: "(" + modelData.count + ")"
                            checked: modManager.nexusSelectedCategories.indexOf(modelData.name) !== -1
                            onToggled: modManager.toggleNexusCategory(modelData.name)
                        }
                    }
                }

                NexusFilterDivider { }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    NexusFilterSectionLabel {
                        text: "TAGS"
                    }

                    Text {
                        text: "Includes"
                        font.pixelSize: 11
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                    }

                    NexusFilterComboBox {
                        panelColor: page.deepPanelColor
                        id: includeTagCombo

                        model: [{ "name": "", "label": "Any Tag" }].concat(modManager.nexusTagOptions)
                        onActivated: (index) => {
                            const entry = includeTagCombo.model[index]
                            modManager.setNexusIncludeTag(entry && entry.name ? entry.name : "")
                        }
                    }

                    Text {
                        text: "Excludes"
                        font.pixelSize: 11
                        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.7)
                    }

                    NexusFilterComboBox {
                        panelColor: page.deepPanelColor
                        id: excludeTagCombo
                        model: [{ "name": "", "label": "None" }].concat(modManager.nexusTagOptions)
                        onActivated: (index) => {
                            const entry = excludeTagCombo.model[index]
                            modManager.setNexusExcludeTag(entry && entry.name ? entry.name : "")
                        }
                    }
                }

                NexusFilterDivider { }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    NexusFilterSectionLabel {
                        text: "SEARCH PARAMETERS"
                    }

                    NexusFilterTextField {
                        id: titleParamInput
                        placeholderText: "Title contains"
                    }

                    NexusFilterTextField {
                        id: descParamInput
                        placeholderText: "Description contains"
                    }

                    NexusFilterTextField {
                        id: authorParamInput
                        placeholderText: "Author contains"
                    }

                    NexusFilterTextField {
                        id: uploaderParamInput
                        placeholderText: "Uploader contains"
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        Layout.topMargin: 4
                        radius: 8
                        color: applyMouse.pressed
                            ? Qt.darker(window.primaryColor, 1.18)
                            : (applyMouse.containsMouse ? Qt.darker(window.primaryColor, 1.08) : window.primaryColor)
                        Behavior on color { ColorAnimation { duration: 120 } }

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 6

                            Image {
                                width: 14
                                height: 14
                                source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                sourceSize: Qt.size(14, 14)
                                smooth: true

                                layer.enabled: true
                                layer.effect: ColorOverlay {
                                    color: "#FFFFFF"
                                }
                            }

                            Text {
                                text: "Apply Filters"
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                color: "#FFFFFF"
                            }
                        }

                        MouseArea {
                            id: applyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                modManager.applyNexusSearchParameters(
                                    titleParamInput.text,
                                    descParamInput.text,
                                    authorParamInput.text,
                                    uploaderParamInput.text
                                )
                            }
                        }
                    }
                }

                NexusFilterDivider { }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    NexusFilterSectionLabel {
                        text: "LANGUAGE SUPPORT"
                    }

                    NexusFilterCheckBox {
                        label: "Hide translations"
                        checked: modManager.nexusHideTranslations
                        onToggled: modManager.setNexusHideTranslations(!modManager.nexusHideTranslations)
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3) }

                    Repeater {
                        model: modManager.nexusLanguagesList

                        delegate: NexusFilterCheckBox {
                            label: modelData.name
                            countText: "(" + modelData.count + ")"
                            dimmed: modelData.count === 0
                            checked: modManager.nexusSelectedLanguage === modelData.name
                            onToggled: {
                                if (modManager.nexusSelectedLanguage === modelData.name) {
                                    modManager.setNexusSelectedLanguage("")
                                } else {
                                    modManager.setNexusSelectedLanguage(modelData.name)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
