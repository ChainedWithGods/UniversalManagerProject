import QtQuick
import QtQuick.Layouts
import QtQuick.Effects
import QtQuick.Shapes
import Qt5Compat.GraphicalEffects

Rectangle {
    id: bundleCard

    required property string bundleId
    property string bundleName: ""
    property int memberCount: 0
    property int activeCount: 0
    property string sizeText: ""
    property string dateText: ""
    property string folderPath: ""
    property var tags: []

    signal openRequested()
    signal toggleRequested(bool enable)
    signal folderRequested()
    signal addTagRequested()

    readonly property bool allActive: memberCount > 0 && activeCount === memberCount
    readonly property color accent: modManager.theme === "Cozy" ? window.primaryColor : window.accentColor
    readonly property color idColorObj: modManager.idColor
    readonly property color tagColorObj: modManager.tagColor
    readonly property var nexusUpdate: {
        var revision = modManager.nexusUpdatesRevision
        return modManager.nexusUpdateForBundle(bundleId)
    }
    readonly property string updateVersionText: {
        var version = String(nexusUpdate.latestVersion || "").trim()
        if (version === "")
            return ""
        if (version.charAt(0) === "v" || version.charAt(0) === "V")
            return "V" + version.slice(1)
        return "V" + version
    }

    width: parent ? parent.width : 700
    height: 256
    radius: 12
    color: window.surfaceColor
    border.width: hoverHandler.hovered ? 2 : 1
    border.color: hoverHandler.hovered
        ? Qt.rgba(accent.r, accent.g, accent.b, 0.85)
        : Qt.rgba(accent.r, accent.g, accent.b, 0.35)

    Behavior on border.width { NumberAnimation { duration: 150 } }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: Qt.rgba(0, 0, 0, 0.22)
        shadowHorizontalOffset: 0
        shadowVerticalOffset: 4
        shadowBlur: 0.8
    }

    HoverHandler { id: hoverHandler }

    MouseArea {
        anchors.fill: parent
        z: -1
        cursorShape: Qt.PointingHandCursor
        onClicked: bundleCard.openRequested()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 16

        Item {
            id: previewArea
            Layout.preferredWidth: 382
            Layout.preferredHeight: 240
            Layout.alignment: Qt.AlignTop

            readonly property real tabHeight: 26
            readonly property real tabRadius: 8
            readonly property string folderText: bundleCard.folderLabel()
            readonly property real tabWidth: Math.min(46 + folderText.length * 6.2, width - 34)
            readonly property real panelHeight: height - tabHeight

            Shape {
                anchors.fill: parent

                ShapePath {
                    strokeColor: window.borderColor
                    strokeWidth: 1
                    fillColor: Qt.rgba(bundleCard.accent.r, bundleCard.accent.g,
                                       bundleCard.accent.b, 0.10)

                    startX: 0
                    startY: previewArea.tabRadius
                    PathQuad { x: previewArea.tabRadius; y: 0; controlX: 0; controlY: 0 }
                    PathLine { x: previewArea.tabWidth - previewArea.tabRadius; y: 0 }
                    PathQuad {
                        x: previewArea.tabWidth; y: previewArea.tabRadius
                        controlX: previewArea.tabWidth; controlY: 0
                    }
                    PathLine { x: previewArea.tabWidth; y: previewArea.tabHeight }
                    PathLine { x: previewArea.width - previewArea.tabRadius; y: previewArea.tabHeight }
                    PathQuad {
                        x: previewArea.width; y: previewArea.tabHeight + previewArea.tabRadius
                        controlX: previewArea.width; controlY: previewArea.tabHeight
                    }
                    PathLine { x: previewArea.width; y: previewArea.height - previewArea.tabRadius }
                    PathQuad {
                        x: previewArea.width - previewArea.tabRadius; y: previewArea.height
                        controlX: previewArea.width; controlY: previewArea.height
                    }
                    PathLine { x: previewArea.tabRadius; y: previewArea.height }
                    PathQuad {
                        x: 0; y: previewArea.height - previewArea.tabRadius
                        controlX: 0; controlY: previewArea.height
                    }
                    PathLine { x: 0; y: previewArea.tabRadius }
                }
            }

            RowLayout {
                x: 9
                y: 0
                width: previewArea.tabWidth - 18
                height: previewArea.tabHeight
                spacing: 5

                Image {
                    Layout.preferredWidth: 11
                    Layout.preferredHeight: 11
                    source: "qrc:/UniversalModManager/Lucide Icons/folder.svg"
                    sourceSize: Qt.size(22, 22)
                    layer.enabled: true
                    layer.effect: ColorOverlay { color: bundleCard.accent }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: previewArea.folderText
                    font.family: "JetBrains Mono"
                    font.pixelSize: 10
                    color: modManager.theme === "Cozy" ? "#6F5F4E" : "#B8C4D6"
                    elide: Text.ElideMiddle
                }
            }

            Rectangle {
                id: tileOne
                width: 88
                height: 112
                radius: 10
                color: window.surfaceColor
                border.width: 1
                border.color: window.borderColor
                rotation: -10
                antialiasing: true
                x: previewArea.width / 2 - width / 2 - 42
                y: previewArea.tabHeight + previewArea.panelHeight / 2 - height / 2 + 5

                Image {
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                    sourceSize: Qt.size(64, 64)
                    smooth: true
                    mipmap: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.effect: ColorOverlay { color: bundleCard.accent }
                }
            }

            Rectangle {
                id: tileTwo
                width: 88
                height: 112
                radius: 10
                color: window.surfaceColor
                border.width: 1
                border.color: window.borderColor
                rotation: 2
                antialiasing: true
                x: previewArea.width / 2 - width / 2
                y: previewArea.tabHeight + previewArea.panelHeight / 2 - height / 2 - 5

                Image {
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                    sourceSize: Qt.size(64, 64)
                    smooth: true
                    mipmap: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.effect: ColorOverlay { color: bundleCard.accent }
                }
            }

            Rectangle {
                id: tileThree
                width: 88
                height: 112
                radius: 10
                color: window.surfaceColor
                border.width: 1
                border.color: window.borderColor
                rotation: 10
                antialiasing: true
                x: previewArea.width / 2 - width / 2 + 42
                y: previewArea.tabHeight + previewArea.panelHeight / 2 - height / 2 + 5

                Image {
                    width: 32
                    height: 32
                    anchors.centerIn: parent
                    source: "qrc:/UniversalModManager/Lucide Icons/package.svg"
                    sourceSize: Qt.size(64, 64)
                    smooth: true
                    mipmap: true
                    layer.enabled: true
                    layer.smooth: true
                    layer.effect: ColorOverlay { color: bundleCard.accent }
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 0
            spacing: 4

            Item { Layout.preferredHeight: 32 }

            RowLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 8
                Layout.topMargin: 2

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: bundleCard.bundleName
                    font.pixelSize: 20
                    font.weight: Font.Medium
                    color: window.textColor
                    elide: Text.ElideRight
                }

                ModBundleUpdateBadge {
                    visible: bundleCard.nexusUpdate.available === true
                    Layout.preferredWidth: implicitWidth
                    versionLabel: bundleCard.updateVersionText
                    tooltipText: "Click to download version "
                                 + (bundleCard.nexusUpdate.latestVersion || "update")
                    onClicked: {
                        var update = bundleCard.nexusUpdate
                        if (update.available === true)
                            modManager.downloadNexusMod(update.modId, update.latestFileId, update.name)
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: bundleCard.memberCount + (bundleCard.memberCount === 1 ? " mod · " : " mods · ")
                      + bundleCard.activeCount + " active · "
                      + (bundleCard.sizeText !== "" ? bundleCard.sizeText : "0 B")
                      + (bundleCard.dateText !== "" ? " · " + bundleCard.dateText : "")
                font.pixelSize: 12
                color: window.textSecondaryColor
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Layout.topMargin: 5

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: 58
                    radius: 6
                    color: Qt.rgba(bundleCard.accent.r, bundleCard.accent.g, bundleCard.accent.b, 0.14)
                    border.width: 1
                    border.color: Qt.rgba(bundleCard.accent.r, bundleCard.accent.g, bundleCard.accent.b, 0.3)

                    Text {
                        anchors.centerIn: parent
                        text: "Bundle"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: bundleCard.accent
                    }
                }

                Rectangle {
                    Layout.preferredHeight: 22
                    Layout.preferredWidth: bundleIdText.implicitWidth + 14
                    radius: 6
                    color: Qt.rgba(bundleCard.idColorObj.r, bundleCard.idColorObj.g, bundleCard.idColorObj.b, 0.14)
                    border.width: 1
                    border.color: Qt.rgba(bundleCard.idColorObj.r, bundleCard.idColorObj.g, bundleCard.idColorObj.b, 0.35)

                    Text {
                        id: bundleIdText
                        anchors.centerIn: parent
                        text: "#" + bundleCard.bundleId
                        font.family: "JetBrains Mono"
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                        color: bundleCard.idColorObj
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                Layout.topMargin: 10
                color: window.borderColor
                opacity: 0.55
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 7
                spacing: 8

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    text: "TAGS"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 0.7
                    color: window.textSecondaryColor
                }

                Flow {
                    Layout.fillWidth: true
                    spacing: 5

                    Repeater {
                        model: Math.min(4, bundleCard.tags.length)

                        delegate: Rectangle {
                            height: 24
                            width: Math.min(tagLabel.implicitWidth + 26, 142)
                            radius: 6
                            color: Qt.rgba(bundleCard.tagColorObj.r, bundleCard.tagColorObj.g,
                                           bundleCard.tagColorObj.b, 0.12)
                            border.width: 1
                            border.color: Qt.rgba(bundleCard.tagColorObj.r, bundleCard.tagColorObj.g,
                                                  bundleCard.tagColorObj.b, 0.3)

                            Row {
                                anchors.centerIn: parent
                                spacing: 4

                                Image {
                                    width: 10
                                    height: 10
                                    anchors.verticalCenter: parent.verticalCenter
                                    source: "qrc:/UniversalModManager/Lucide Icons/tag.svg"
                                    sourceSize: Qt.size(20, 20)
                                    layer.enabled: true
                                    layer.effect: ColorOverlay { color: bundleCard.tagColorObj }
                                }

                                Text {
                                    id: tagLabel
                                    width: Math.min(implicitWidth, 106)
                                    text: bundleCard.tags[index] || ""
                                    font.pixelSize: 10
                                    font.weight: Font.Medium
                                    color: bundleCard.tagColorObj
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: bundleCard.tags.length > 4
                        height: 24
                        width: moreTagsLabel.implicitWidth + 18
                        radius: 6
                        color: "transparent"
                        border.width: 1
                        border.color: window.borderColor

                        Text {
                            id: moreTagsLabel
                            anchors.centerIn: parent
                            text: "+" + (bundleCard.tags.length - 4)
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: window.textSecondaryColor
                        }
                    }

                    Rectangle {
                        id: addTagChip
                        height: 24
                        width: addTagLabel.implicitWidth + 28
                        radius: 6
                        color: addTagHover.hovered
                            ? Qt.rgba(bundleCard.accent.r, bundleCard.accent.g, bundleCard.accent.b, 0.10)
                            : "transparent"
                        border.width: 1
                        border.color: Qt.rgba(window.borderColor.r, window.borderColor.g,
                                              window.borderColor.b, 0.9)

                        HoverHandler { id: addTagHover; cursorShape: Qt.PointingHandCursor }

                        Row {
                            anchors.centerIn: parent
                            spacing: 4

                            Image {
                                width: 10
                                height: 10
                                anchors.verticalCenter: parent.verticalCenter
                                source: "qrc:/UniversalModManager/Lucide Icons/plus.svg"
                                sourceSize: Qt.size(20, 20)
                                layer.enabled: true
                                layer.effect: ColorOverlay { color: window.textSecondaryColor }
                            }

                            Text {
                                id: addTagLabel
                                text: "Add tag"
                                font.pixelSize: 10
                                font.weight: Font.Medium
                                color: window.textSecondaryColor
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            onClicked: bundleCard.addTagRequested()
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 14
                spacing: 8

                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 36
                    radius: 8
                    color: openHover.hovered ? Qt.lighter(bundleCard.accent, 1.12) : bundleCard.accent

                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: openHover; cursorShape: Qt.PointingHandCursor }

                    Text {
                        anchors.centerIn: parent
                        text: "Open bundle"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: modManager.theme === "Cozy" ? "#FFFFFF" : "#241A10"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: bundleCard.openRequested()
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 36
                    radius: 8
                    color: toggleHover.hovered
                        ? Qt.lighter(modManager.disableButtonColor, 1.12)
                        : modManager.disableButtonColor

                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: toggleHover; cursorShape: Qt.PointingHandCursor }

                    Text {
                        anchors.centerIn: parent
                        text: bundleCard.allActive ? "Disable all" : "Enable all"
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        color: "#FFFFFF"
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: bundleCard.toggleRequested(!bundleCard.allActive)
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 90
                    Layout.preferredHeight: 32
                    radius: 8
                    color: folderHover.hovered
                        ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                        : Qt.rgba(window.cardColor1.r, window.cardColor1.g, window.cardColor1.b, 0.5)
                    border.width: 1
                    border.color: Qt.rgba(window.borderColor.r, window.borderColor.g, window.borderColor.b, 0.3)

                    Behavior on color { ColorAnimation { duration: 150 } }
                    HoverHandler { id: folderHover; cursorShape: Qt.PointingHandCursor }

                    Row {
                        anchors.centerIn: parent
                        spacing: 5

                        Image {
                            width: 14
                            height: 14
                            anchors.verticalCenter: parent.verticalCenter
                            source: "qrc:/UniversalModManager/Lucide Icons/folder-open.svg"
                            sourceSize: Qt.size(28, 28)
                            layer.enabled: true
                            layer.effect: ColorOverlay { color: window.textColor }
                            opacity: 0.8
                        }

                        Text {
                            text: "Folder"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: window.textColor
                            opacity: 0.8
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: bundleCard.folderRequested()
                    }
                }

                Item { Layout.fillWidth: true }

                Image {
                    Layout.preferredWidth: 16
                    Layout.preferredHeight: 16
                    source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                    sourceSize: Qt.size(32, 32)
                    layer.enabled: true
                    layer.effect: ColorOverlay { color: window.textSecondaryColor }
                }
            }
        }
    }

    function folderLabel() {
        var path = String(folderPath || "").replace(/\\/g, "/")
        var parts = path.split("/")
        while (parts.length > 0 && parts[parts.length - 1] === "")
            parts.pop()
        if (parts.length >= 2)
            return parts[parts.length - 2] + "/" + parts[parts.length - 1]
        return parts.length > 0 ? parts[parts.length - 1] : ""
    }
}
