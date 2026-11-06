import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: bundleRow

    required property var bundleData
    property int statusRevision: 0
    property bool expanded: false
    property var selectedMods: []

    property string memberAnchor: ""
    readonly property int rowHeight: 72
    readonly property int childrenHeight: expanded ? childrenColumn.implicitHeight + 16 : 0

    signal toggleRequested(bool enable)
    signal folderRequested()
    signal expansionRequested(bool expanded)
    signal modActivated(string modName)
    signal bundleSelectionRequested(var memberNames, bool additive)
    signal modSelectionRequested(string modName, bool additive)

    readonly property color accent: modManager.theme === "Cozy" ? window.primaryColor : window.accentColor
    readonly property int activeCount: bundleData ? (bundleData.activeCount || 0) : 0
    readonly property int memberCount: bundleData ? (bundleData.memberCount || 0) : 0
    readonly property bool allActive: memberCount > 0 && activeCount === memberCount
    readonly property bool noneActive: activeCount === 0
    readonly property bool bundleSelected: {
        if (!members || members.length === 0)
            return false
        for (var i = 0; i < members.length; ++i) {
            if (selectedMods.indexOf(members[i]) === -1)
                return false
        }
        return true
    }
    readonly property color badgeFillColor: noneActive
        ? Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g,
                  window.textSecondaryColor.b, 0.18)
        : accent
    readonly property color badgeTextColor: {
        if (noneActive)
            return window.textSecondaryColor
        return (0.299 * accent.r + 0.587 * accent.g + 0.114 * accent.b) > 0.55
            ? "#1A1A1A" : "#FFFFFF"
    }
    readonly property var nexusUpdate: {
        var revision = modManager.nexusUpdatesRevision
        return bundleData ? modManager.nexusUpdateForBundle(bundleData.id) : ({})
    }
    readonly property var members: {
        var revision = statusRevision
        return bundleData ? (bundleData.members || []) : []
    }

    function tintedSurface(base, tint) {
        return Qt.rgba(base.r * (1 - tint.a) + tint.r * tint.a,
                       base.g * (1 - tint.a) + tint.g * tint.a,
                       base.b * (1 - tint.a) + tint.b * tint.a,
                       base.a)
    }

    width: parent ? parent.width : 700
    height: rowHeight + childrenHeight
    radius: 12

    color: (modManager.theme === "Glassmorphic" || modManager.theme === "Crimson")
        ? window.surfaceColor
        : bundleRow.tintedSurface(window.surfaceColor, window.cardColor1)
    border.width: 1
    border.color: window.borderColor

    Rectangle {
        id: headerBackground
        width: parent.width
        height: bundleRow.rowHeight
        radius: 12
        color: bundleRow.bundleSelected
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.22)
            : (headerHover.hovered
                ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.05)
                : "transparent")
        border.width: 0
        border.color: bundleRow.bundleSelected
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.9)
            : Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b,
                      headerHover.hovered ? 0.55 : 0.3)

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }
        HoverHandler { id: headerHover }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: window.borderColor
        }
    }

    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: bundleRow.rowHeight
        z: -1
        onClicked: function(mouse) {
            bundleRow.bundleSelectionRequested(
                bundleRow.members,
                (mouse.modifiers & Qt.ControlModifier) !== 0)
        }
        onDoubleClicked: bundleRow.expansionRequested(!bundleRow.expanded)
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: bundleRow.rowHeight
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 15

        Item {
            Layout.preferredWidth: 40
            Layout.preferredHeight: 40
            Layout.alignment: Qt.AlignVCenter

            Rectangle {
                anchors.centerIn: parent
                width: 28
                height: 28
                radius: 7
                color: chevronHover.hovered
                    ? Qt.rgba(bundleRow.accent.r, bundleRow.accent.g, bundleRow.accent.b, 0.14)
                    : "transparent"

                HoverHandler { id: chevronHover; cursorShape: Qt.PointingHandCursor }

                Image {
                    anchors.centerIn: parent
                    width: 15
                    height: 15
                    source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                    sourceSize: Qt.size(15, 15)
                    rotation: bundleRow.expanded ? 90 : 0
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: bundleRow.accent
                    }

                    Behavior on rotation { NumberAnimation { duration: 120 } }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: bundleRow.expansionRequested(!bundleRow.expanded)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.preferredWidth: 300
            Layout.minimumWidth: 0
            spacing: 8

            Text {
                text: bundleRow.bundleData ? (bundleRow.bundleData.name || "") : ""
                font.pixelSize: 14
                font.weight: Font.Medium
                color: window.textColor
                elide: Text.ElideRight
            }

            UpdateAvailableBadge {
                compact: true
                label: "Update"
                visible: bundleRow.nexusUpdate.available === true
                tooltipText: "Click to download version " + (bundleRow.nexusUpdate.latestVersion || "update")
                onClicked: {
                    var update = bundleRow.nexusUpdate
                    if (update.available === true)
                        modManager.downloadNexusMod(update.modId, update.latestFileId, update.name)
                }
            }

            Item { Layout.fillWidth: true }

            Rectangle {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                radius: 6
                color: toggleHover.hovered
                    ? Qt.rgba(bundleRow.accent.r, bundleRow.accent.g, bundleRow.accent.b, 0.14)
                    : "transparent"

                HoverHandler { id: toggleHover; cursorShape: Qt.PointingHandCursor }

                Image {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    source: bundleRow.allActive
                        ? "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                        : "qrc:/UniversalModManager/Lucide Icons/play.svg"
                    sourceSize: Qt.size(13, 13)
                    layer.enabled: true
                    layer.effect: ColorOverlay {
                        color: bundleRow.allActive ? "#F59E0B" : "#4CAF50"
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: bundleRow.toggleRequested(!bundleRow.allActive)
                }
            }

            Rectangle {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                radius: 6
                color: folderHover.hovered
                    ? Qt.rgba(bundleRow.accent.r, bundleRow.accent.g, bundleRow.accent.b, 0.14)
                    : "transparent"

                HoverHandler { id: folderHover; cursorShape: Qt.PointingHandCursor }

                Image {
                    anchors.centerIn: parent
                    width: 13
                    height: 13
                    source: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                    sourceSize: Qt.size(13, 13)
                    layer.enabled: true
                    layer.effect: ColorOverlay { color: window.textSecondaryColor }
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: bundleRow.folderRequested()
                }
            }
        }

        Rectangle {
            Layout.preferredWidth: 105
            Layout.preferredHeight: 36
            radius: 18
            color: bundleRow.badgeFillColor

            Text {
                anchors.centerIn: parent
                text: bundleRow.noneActive
                    ? "● Disabled"
                    : "● " + bundleRow.activeCount + " / " + bundleRow.memberCount
                font.pixelSize: 11
                font.weight: Font.Medium
                color: bundleRow.badgeTextColor
            }
        }

        Text {
            Layout.preferredWidth: 100
            text: "Bundle"
            font.pixelSize: 13
            font.weight: Font.Bold
            color: bundleRow.accent
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.preferredWidth: 100
            text: bundleRow.memberCount + (bundleRow.memberCount === 1 ? " mod" : " mods")
            font.pixelSize: 13
            color: window.textSecondaryColor
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            Layout.preferredWidth: 120
            text: bundleRow.bundleData ? (bundleRow.bundleData.size || "N/A") : "N/A"
            font.pixelSize: 13
            color: window.textSecondaryColor
            horizontalAlignment: Text.AlignRight
        }

        Text {
            Layout.preferredWidth: 110
            text: bundleRow.bundleData ? (bundleRow.bundleData.date || "N/A") : "N/A"
            font.pixelSize: 13
            color: window.textSecondaryColor
            horizontalAlignment: Text.AlignRight
        }
    }

    Rectangle {
        id: childrenWell
        anchors.top: parent.top
        anchors.topMargin: bundleRow.rowHeight + 4
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        height: childrenColumn.implicitHeight + 8
        radius: 10

        color: modManager.theme === "Crimson"
            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                       window.primaryColor.b, 0.10)
            : (window.bgColor.a > 0
                ? window.bgColor
                : Qt.rgba(0, 0, 0, 0.16))
        border.width: 1
        border.color: Qt.rgba(window.borderColor.r, window.borderColor.g,
                              window.borderColor.b, window.borderColor.a * 0.7)
        visible: bundleRow.expanded
    }

    Column {
        id: childrenColumn
        anchors.top: parent.top
        anchors.topMargin: bundleRow.rowHeight + 8
        anchors.left: parent.left
        anchors.leftMargin: 8
        anchors.right: parent.right
        anchors.rightMargin: 8
        spacing: 0
        visible: bundleRow.expanded

        Repeater {
            model: bundleRow.expanded ? bundleRow.members.length : 0

            delegate: Rectangle {
                id: childRow

                readonly property string modName: bundleRow.members[index] || ""
                readonly property var details: {
                    var revision = bundleRow.statusRevision
                    return modManager.getModDetails(modName)
                }
                readonly property bool modEnabled: details.enabled === true
                readonly property bool selected: bundleRow.selectedMods.indexOf(modName) !== -1
                property real checkProgress: modEnabled ? 1 : 0

                Behavior on checkProgress {
                    NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                }

                width: parent ? parent.width : 0
                height: 58
                radius: 10
                color: "transparent"
                border.width: selected ? 1 : 0
                border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                      window.primaryColor.b, 0.4)

                Rectangle {
                    anchors.fill: parent
                    radius: parent.radius
                    color: Qt.rgba(window.primaryColor.r, window.primaryColor.g,
                                   window.primaryColor.b,
                                   childRow.selected
                                       ? 0.12
                                       : (modManager.theme === "Cozy" ? 0.07 : 0.05))
                    opacity: childRow.selected ? 1.0 : (childHover.hovered ? 1.0 : 0.0)
                    Behavior on opacity { NumberAnimation { duration: 130 } }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 68
                    anchors.rightMargin: 16
                    anchors.top: parent.top
                    height: 1
                    color: Qt.rgba(window.borderColor.r, window.borderColor.g,
                                   window.borderColor.b, window.borderColor.a * 0.7)
                    visible: index > 0
                    z: 2
                }

                HoverHandler { id: childHover }

                MouseArea {
                    anchors.fill: parent
                    z: -1
                    onClicked: function(mouse) {
                        var ctrl = (mouse.modifiers & Qt.ControlModifier) !== 0
                        var shift = (mouse.modifiers & Qt.ShiftModifier) !== 0
                        if (shift && bundleRow.memberAnchor !== "") {
                            var from = bundleRow.members.indexOf(bundleRow.memberAnchor)
                            var to = bundleRow.members.indexOf(childRow.modName)
                            if (from >= 0 && to >= 0) {
                                var start = Math.min(from, to)
                                var end = Math.max(from, to)
                                var range = []
                                for (var i = start; i <= end; ++i)
                                    range.push(bundleRow.members[i])
                                bundleRow.bundleSelectionRequested(range, ctrl)
                                return
                            }
                        }
                        bundleRow.memberAnchor = childRow.modName
                        bundleRow.modSelectionRequested(childRow.modName, ctrl)
                    }
                    onDoubleClicked: bundleRow.modActivated(childRow.modName)
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 15

                    Item {
                        Layout.preferredWidth: 48
                        Layout.fillHeight: true
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            x: 8
                            width: 1

                            readonly property bool isFirst: index === 0
                            readonly property bool isLast: index === bundleRow.members.length - 1
                            y: isFirst ? -8 : 0
                            height: {
                                if (isFirst && isLast)
                                    return parent.height / 2 + 8
                                if (isFirst)
                                    return parent.height + 8
                                if (isLast)
                                    return parent.height / 2
                                return parent.height
                            }
                            color: Qt.rgba(bundleRow.accent.r, bundleRow.accent.g,
                                           bundleRow.accent.b, 0.4)
                        }

                        Rectangle {
                            x: 8
                            y: parent.height / 2
                            width: 12
                            height: 1
                            color: Qt.rgba(bundleRow.accent.r, bundleRow.accent.g,
                                           bundleRow.accent.b, 0.4)
                        }

                        Image {
                            x: 20
                            y: (parent.height - height) / 2
                            width: 20
                            height: 20
                            source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                            sourceSize: Qt.size(40, 40)
                            layer.enabled: true
                            layer.effect: ColorOverlay { color: bundleRow.accent }
                        }
                    }

                    Item {
                        Layout.preferredWidth: 22
                        Layout.preferredHeight: 1
                    }

                    Text {
                        Layout.fillWidth: true
                        Layout.preferredWidth: 300
                        Layout.minimumWidth: 0
                        text: childRow.modName
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        color: window.textColor
                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.preferredWidth: 105
                        Layout.preferredHeight: 30
                        Layout.alignment: Qt.AlignVCenter

                        Rectangle {
                            width: 28
                            height: 28
                            anchors.centerIn: parent
                            radius: 8
                            color: childRow.modEnabled
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g,
                                          window.accentColor.b, checkHover.hovered ? 0.2 : 0.12)
                                : (checkHover.hovered
                                    ? Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g,
                                              window.textSecondaryColor.b, 0.08)
                                    : "transparent")
                            border.width: 1
                            border.color: childRow.modEnabled
                                ? Qt.rgba(window.accentColor.r, window.accentColor.g,
                                          window.accentColor.b, 0.75)
                                : Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g,
                                          window.textSecondaryColor.b, 0.35)

                            Behavior on color { ColorAnimation { duration: 150 } }
                            Behavior on border.color { ColorAnimation { duration: 150 } }

                            HoverHandler { id: checkHover; cursorShape: Qt.PointingHandCursor }

                            Image {
                                anchors.centerIn: parent
                                width: 18
                                height: 18
                                source: "qrc:/UniversalModManager/Lucide Icons/check.svg"
                                sourceSize: Qt.size(36, 36)
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                visible: childRow.checkProgress > 0.001
                                opacity: childRow.checkProgress
                                scale: 0.8 + 0.2 * childRow.checkProgress
                                layer.enabled: true
                                layer.smooth: true
                                layer.textureSize: Qt.size(width * 2, height * 2)
                                layer.effect: ColorOverlay { color: window.accentColor }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: modManager.toggleModStatus(childRow.modName)
                            }
                        }
                    }

                    Text {
                        Layout.preferredWidth: 100
                        text: "Folder"
                        font.pixelSize: 12
                        color: "#2196F3"
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.preferredWidth: 100
                        text: (childRow.details.fileCount || 0) + ((childRow.details.fileCount || 0) === 1 ? " file" : " files")
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.preferredWidth: 120
                        text: childRow.details.size || "N/A"
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        horizontalAlignment: Text.AlignRight
                    }

                    Text {
                        Layout.preferredWidth: 110
                        text: childRow.details.installDate || "N/A"
                        font.pixelSize: 12
                        color: window.textSecondaryColor
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
        }
    }
}
