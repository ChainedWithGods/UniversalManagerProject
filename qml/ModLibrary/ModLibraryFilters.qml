pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Item {
    id: root

    property string mode: "views"
    property string viewMode: "cards"
    property var activeFilters: []
    property string sortBy: "name"

    signal viewModeRequested(string mode)
    signal filtersChanged(var filters)
    signal sortRequested(string field)
    signal checkUpdatesRequested()

    implicitWidth: mode === "views" ? 300 : 0
    implicitHeight: mode === "views" ? 44 : 36

    Rectangle {
        anchors.fill: parent
        visible: root.mode === "views"
        color: modManager.theme === "Glassmorphic"
            ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                      modManager.accentColor.b, 0.25)
            : window.surfaceColor
        radius: window.cardRadius / 2
        border.color: window.borderColor
        border.width: window.borderWidth

        RowLayout {
            anchors.fill: parent
            anchors.margins: 4
            spacing: 4

            component ViewButton: Button {
                id: viewButton
                property string label
                property string targetMode
                readonly property bool selected: root.viewMode === targetMode

                Layout.fillWidth: true
                Layout.fillHeight: true
                flat: true
                hoverEnabled: true
                focusPolicy: Qt.NoFocus

                contentItem: Text {
                    text: viewButton.label
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    color: viewButton.selected ? window.primaryColor : window.textSecondaryColor
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }

                background: Rectangle {
                    color: {
                        if (viewButton.selected) {
                            return modManager.theme === "Glassmorphic"
                                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                          window.primaryColor.b, 0.2)
                                : Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                          window.primaryColor.b, 0.15)
                        }
                        if (viewButton.hovered) {
                            if (modManager.theme === "Wavey" || modManager.theme === "Crimson")
                                return Qt.rgba(1, 1, 1, 0.15)
                            if (modManager.theme === "Glassmorphic")
                                return Qt.rgba(1, 1, 1, 0.1)
                            return Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                            window.primaryColor.b, 0.08)
                        }
                        return "transparent"
                    }
                    radius: 8
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                onClicked: root.viewModeRequested(viewButton.targetMode)
            }

            ViewButton { label: "Cards"; targetMode: "cards" }
            ViewButton { label: "List"; targetMode: "details" }
            ViewButton { label: "Collections"; targetMode: "collections" }
            ViewButton { label: "Bundles"; targetMode: "bundles" }
        }
    }

    RowLayout {
        anchors.fill: parent
        visible: root.mode === "filters"
        spacing: 12

        Flow {
            Layout.fillWidth: true
            spacing: 8
            visible: modManager.modList && modManager.modList.length > 0

            component FilterChip: Rectangle {
                property string filterName
                property string displayText
                readonly property bool isActive: root.activeFilters.indexOf(filterName) !== -1

                width: chipText.contentWidth + 44
                height: 36
                radius: 18
                color: isActive
                    ? (modManager.theme === "Glassmorphic"
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                  window.primaryColor.b, 0.3)
                        : window.primaryColor)
                    : (modManager.theme === "Glassmorphic"
                        ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                                  modManager.accentColor.b, 0.15)
                        : window.surfaceColor)
                border.width: 1
                border.color: isActive ? window.primaryColor : window.borderColor

                Behavior on color { ColorAnimation { duration: 150 } }
                Behavior on border.color { ColorAnimation { duration: 150 } }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    onClicked: {
                        var filters = root.activeFilters.slice()
                        var idx = filters.indexOf(filterName)
                        if (idx === -1)
                            filters.push(filterName)
                        else
                            filters.splice(idx, 1)
                        root.filtersChanged(filters)
                    }
                }

                Text {
                    id: chipText
                    anchors.centerIn: parent
                    text: displayText
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: isActive
                        ? (modManager.theme === "Glassmorphic" ? modManager.accentColor : "#FFFFFF")
                        : window.textColor
                }
            }

            FilterChip { filterName: "active"; displayText: "Active" }
            FilterChip { filterName: "disabled"; displayText: "Disabled" }
            FilterChip {
                filterName: "favorite"
                displayText: "Favorites"
                visible: root.viewMode !== "collections"
            }
            FilterChip {
                filterName: "folder"
                displayText: "Folder Mods"
                visible: root.viewMode !== "collections"
            }
            FilterChip {
                filterName: "loose"
                displayText: "Loose Files"
                visible: root.viewMode !== "collections"
            }

            Rectangle {
                visible: root.activeFilters.length > 0
                width: clearText.contentWidth + 32
                height: 36
                radius: 18
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                              modManager.accentColor.b, 0.1)
                    : Qt.rgba(0, 0, 0, 0.05)
                border.width: 1
                border.color: window.textSecondaryColor

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.filtersChanged([])
                }

                Text {
                    id: clearText
                    anchors.centerIn: parent
                    text: "✕ Clear"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: window.textSecondaryColor
                }
            }

            Rectangle {
                width: 140
                height: 36
                color: "transparent"
                radius: window.cardRadius / 2
                border.color: "transparent"
                border.width: 0
                visible: root.viewMode !== "collections"

                ComboBox {
                    id: sortComboBox
                    anchors.fill: parent
                    indicator: Item { }
                    model: ["Name", "Date", "Size", "Status"]

                    onCurrentIndexChanged: {
                        if (currentIndex === 0) root.sortRequested("name")
                        else if (currentIndex === 1) root.sortRequested("date")
                        else if (currentIndex === 2) root.sortRequested("size")
                        else if (currentIndex === 3) root.sortRequested("status")
                    }

                    contentItem: Item {
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 8
                            spacing: 6

                            Text {
                                text: "⇅"
                                font.pixelSize: 14
                                color: window.textColor
                            }

                            Text {
                                text: sortComboBox.displayText
                                font.pixelSize: 13
                                font.weight: Font.Medium
                                color: window.textColor
                            }

                            Item { Layout.fillWidth: true }
                        }
                    }

                    background: Rectangle {
                        readonly property bool open: sortComboBox.popup.visible
                        color: open
                            ? (modManager.theme === "Glassmorphic"
                                ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor)
                            : (modManager.theme === "Glassmorphic"
                                ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                                          modManager.accentColor.b, 0.15)
                                : window.surfaceColor)
                        border.color: open || sortComboBox.activeFocus
                            ? window.primaryColor : window.borderColor
                        border.width: 1
                        radius: 18
                        Behavior on color { ColorAnimation { duration: 150 } }
                        Behavior on border.color { ColorAnimation { duration: 150 } }
                    }

                    popup: Popup {
                        y: sortComboBox.height + 4
                        width: sortComboBox.width
                        implicitHeight: contentItem.implicitHeight + 8
                        padding: 4

                        background: Rectangle {
                            color: modManager.theme === "Glassmorphic"
                                ? Qt.rgba(0.12, 0.12, 0.12, 0.98) : window.surfaceColor
                            radius: 10
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

                        contentItem: ListView {
                            clip: true
                            implicitHeight: contentHeight
                            model: sortComboBox.popup.visible ? sortComboBox.delegateModel : null
                            currentIndex: sortComboBox.highlightedIndex
                            ScrollIndicator.vertical: ScrollIndicator { }
                        }
                    }

                    delegate: ItemDelegate {
                        required property var modelData
                        width: sortComboBox.width - 8
                        height: 36
                        padding: 0
                        leftPadding: 0
                        rightPadding: 0
                        topPadding: 0
                        bottomPadding: 0

                        contentItem: Text {
                            text: modelData
                            color: (parent.hovered || parent.highlighted) ? "#FFFFFF" : window.textColor
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignLeft
                            leftPadding: 12
                            rightPadding: 12
                        }

                        background: Rectangle {
                            anchors.fill: parent
                            color: (parent.hovered || parent.highlighted)
                                ? window.accentColor : "transparent"
                            radius: 6

                            Behavior on color { ColorAnimation { duration: 100 } }
                        }
                    }
                }
            }

            Rectangle {
                id: checkUpdatesChip
                visible: root.viewMode !== "collections"
                    && (nexusOAuth.authenticated || modManager.nexusApiKey.trim().length > 0)
                width: checkUpdatesRow.implicitWidth + 24
                height: 36
                radius: 18
                color: modManager.theme === "Glassmorphic"
                    ? Qt.rgba(modManager.accentColor.r, modManager.accentColor.g,
                              modManager.accentColor.b, 0.15)
                    : window.surfaceColor
                border.width: 1
                border.color: modManager.isCheckingNexusUpdates
                    ? window.primaryColor : window.borderColor

                RowLayout {
                    id: checkUpdatesRow
                    anchors.centerIn: parent
                    spacing: 6

                    Item {
                        width: 20
                        height: 20

                        BusyRing {
                            anchors.centerIn: parent
                            running: modManager.isCheckingNexusUpdates
                            ringColor: window.primaryColor
                        }

                        Item {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            visible: !modManager.isCheckingNexusUpdates

                            Image {
                                id: checkUpdatesIcon
                                anchors.fill: parent
                                source: "qrc:/UniversalModManager/Lucide Icons/cloud-download.svg"
                                sourceSize: Qt.size(15, 15)
                                visible: modManager.theme === "Cozy"
                            }

                            ColorOverlay {
                                anchors.fill: checkUpdatesIcon
                                source: checkUpdatesIcon
                                color: window.iconColor
                                visible: modManager.theme !== "Cozy"
                            }
                        }
                    }

                    Text {
                        text: modManager.isCheckingNexusUpdates ? "Checking..." : "Check updates"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: window.textColor
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.checkUpdatesRequested()
                }
            }
        }

        Item { Layout.fillWidth: true }
    }
}
