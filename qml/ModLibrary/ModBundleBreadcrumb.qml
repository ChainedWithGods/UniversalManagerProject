import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: breadcrumb

    required property var bundleData

    signal backRequested()
    signal toggleRequested(bool enable)
    signal folderRequested()
    signal deleteRequested()
    signal addModsRequested()

    readonly property color accent: modManager.theme === "Cozy" ? window.primaryColor : window.accentColor
    readonly property int activeCount: bundleData ? (bundleData.activeCount || 0) : 0
    readonly property int memberCount: bundleData ? (bundleData.memberCount || 0) : 0
    readonly property bool allActive: memberCount > 0 && activeCount === memberCount
    readonly property var nexusUpdate: {
        var revision = modManager.nexusUpdatesRevision
        return bundleData ? modManager.nexusUpdateForBundle(bundleData.id) : ({})
    }
    readonly property string updateVersionText: {
        var version = String(nexusUpdate.latestVersion || "").trim()
        if (version === "")
            return ""
        if (version.charAt(0) === "v" || version.charAt(0) === "V")
            return "V" + version.slice(1)
        return "V" + version
    }
    readonly property real bundleNameMaxWidth: Math.max(0, breadcrumbRow.width
        - breadcrumbRow.anchors.leftMargin - breadcrumbRow.anchors.rightMargin
        - backButton.width - crumbRoot.width - crumbSlash.width
        - bundleChip.width - memberStats.width - actionCluster.width
        - breadcrumbRow.spacing * 7)

    width: parent ? parent.width : 700
    height: 56
    radius: 12
    color: window.surfaceColor
    border.width: 1
    border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.3)

    RowLayout {
        id: breadcrumbRow
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 12
        spacing: 10

        Rectangle {
            id: backButton
            Layout.preferredWidth: 34
            Layout.preferredHeight: 34
            radius: 9
            color: backHover.hovered
                ? Qt.rgba(accent.r, accent.g, accent.b, 0.12)
                : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5)
            border.width: 1
            border.color: window.borderColor

            HoverHandler { id: backHover; cursorShape: Qt.PointingHandCursor }

            Image {
                anchors.centerIn: parent
                source: "qrc:/UniversalModManager/Lucide Icons/arrow-left.svg"
                sourceSize: Qt.size(16, 16)
                layer.enabled: true
                layer.effect: ColorOverlay { color: window.textColor }
            }

            MouseArea {
                anchors.fill: parent
                onClicked: breadcrumb.backRequested()
            }
        }

        Text {
            id: crumbRoot
            text: "Mods Library"
            font.pixelSize: 12
            color: window.textSecondaryColor
        }

        Text {
            id: crumbSlash
            text: "/"
            font.pixelSize: 12
            color: window.textSecondaryColor
            opacity: 0.6
        }

        Text {
            Layout.maximumWidth: breadcrumb.bundleNameMaxWidth
            text: bundleData ? (bundleData.name || "") : ""
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: window.textColor
            elide: Text.ElideRight
        }

        Rectangle {
            id: bundleChip
            Layout.preferredHeight: 24
            Layout.preferredWidth: 58
            radius: 6
            color: Qt.rgba(accent.r, accent.g, accent.b, 0.14)
            border.width: 1
            border.color: Qt.rgba(accent.r, accent.g, accent.b, 0.3)

            Text {
                anchors.centerIn: parent
                text: "Bundle"
                font.pixelSize: 10
                font.weight: Font.Bold
                color: breadcrumb.accent
            }
        }

        Text {
            id: memberStats
            text: memberCount + (memberCount === 1 ? " mod · " : " mods · ") + activeCount + " active"
            font.pixelSize: 12
            color: window.textSecondaryColor
        }

        Item { Layout.fillWidth: true }

        RowLayout {
            id: actionCluster
            Layout.alignment: Qt.AlignVCenter
            spacing: 10

            ModBundleUpdateBadge {
                visible: breadcrumb.nexusUpdate.available === true
                versionLabel: breadcrumb.updateVersionText
                tooltipText: "Click to download version "
                             + (breadcrumb.nexusUpdate.latestVersion || "update")
                onClicked: {
                    var update = breadcrumb.nexusUpdate
                    if (update.available === true)
                        modManager.downloadNexusMod(update.modId, update.latestFileId, update.name)
                }
            }

            ExpandingActionButton {
                label: "Delete"
                iconSource: "qrc:/UniversalModManager/Lucide Icons/trash-2.svg"
                accentColor: window.accentColor
                idleColor: "transparent"
                idleBorderColor: Qt.rgba(window.textColor.r, window.textColor.g,
                                         window.textColor.b, 0.4)
                idleContentColor: window.textColor
                onClicked: breadcrumb.deleteRequested()
            }

            Rectangle {
                Layout.preferredWidth: 98
                Layout.preferredHeight: 32
                radius: 8
                color: addModsHover.hovered
                    ? Qt.rgba(accent.r, accent.g, accent.b, 0.12)
                    : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5)
                border.width: 1
                border.color: window.borderColor

                HoverHandler { id: addModsHover; cursorShape: Qt.PointingHandCursor }

                Row {
                    anchors.centerIn: parent
                    spacing: 5

                    Image {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/UniversalModManager/Lucide Icons/plus.svg"
                        sourceSize: Qt.size(14, 14)
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: window.textColor }
                    }

                    Text {
                        text: "Add Mods"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: window.textColor
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: breadcrumb.addModsRequested()
                }
            }

            Rectangle {
                Layout.preferredWidth: 108
                Layout.preferredHeight: 32
                radius: 8
                color: toggleHover.hovered
                    ? Qt.lighter(modManager.disableButtonColor, 1.12)
                    : modManager.disableButtonColor

                HoverHandler { id: toggleHover; cursorShape: Qt.PointingHandCursor }

                Text {
                    anchors.centerIn: parent
                    text: breadcrumb.allActive ? "Disable all" : "Enable all"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    color: "#FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: breadcrumb.toggleRequested(!breadcrumb.allActive)
                }
            }

            Rectangle {
                Layout.preferredWidth: 88
                Layout.preferredHeight: 32
                radius: 8
                color: folderHover.hovered
                    ? Qt.rgba(accent.r, accent.g, accent.b, 0.12)
                    : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5)
                border.width: 1
                border.color: window.borderColor

                HoverHandler { id: folderHover; cursorShape: Qt.PointingHandCursor }

                Row {
                    anchors.centerIn: parent
                    spacing: 5

                    Image {
                        width: 14
                        height: 14
                        anchors.verticalCenter: parent.verticalCenter
                        source: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                        sourceSize: Qt.size(14, 14)
                        layer.enabled: true
                        layer.effect: ColorOverlay { color: window.textColor }
                    }

                    Text {
                        text: "Folder"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: window.textColor
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: breadcrumb.folderRequested()
                }
            }
        }
    }
}
