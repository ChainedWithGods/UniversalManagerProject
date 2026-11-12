import QtQuick
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

Rectangle {
    id: section

    required property var rootData
    property string folderName: ""
    property string bundleFolderName: ""
    property string originalName: ""

    readonly property var coreFiles: rootData && rootData.coreFiles ? rootData.coreFiles : []
    readonly property var companionFiles: rootData && rootData.companionFiles ? rootData.companionFiles : []
    property bool companionsExpanded: coreFiles.length === 0
    readonly property bool isLogicRoot: rootData && rootData.rootKind === "logic"
    readonly property bool isLuaRoot: rootData && rootData.rootKind === "lua"
    readonly property bool isCppRoot: rootData && rootData.rootKind === "cpp"
    readonly property bool willCreate: rootData && rootData.create === true
    readonly property bool wrapInFolder: rootData && rootData.wrapInFolder === true
    readonly property string rootPath: rootData ? (rootData.rootPath || "") : ""
    readonly property string displayRootPath: {
        if (rootPath === "")
            return ""
        var path = rootPath
        while (path.length > 1 && (path.endsWith("/") || path.endsWith("\\")))
            path = path.slice(0, path.length - 1)
        var bundle = String(bundleFolderName || "").trim()
        if (bundle !== "" && !section.isLuaRoot && !section.isCppRoot)
            path += "/" + bundle
        if (rootData && rootData.wrapInFolder && folderName !== "")
            path += "/" + folderName
        return section.shortPath(path)
    }

    function shortPath(path) {
        var normalized = String(path).replace(/\\/g, "/")
        var lower = normalized.toLowerCase()
        var luaMarker = lower.indexOf("/universal manager lua mods/")
        if (luaMarker >= 0) {
            return "Lua Mods/" + normalized.slice(luaMarker + "/universal manager lua mods/".length)
        }
        var cppMarker = lower.indexOf("/universal manager cpp mods/")
        if (cppMarker >= 0) {
            return "C++ Mods/" + normalized.slice(cppMarker + "/universal manager cpp mods/".length)
        }
        var marker = lower.indexOf("/content/paks/")
        if (marker < 0 && lower.indexOf("content/paks/") === 0)
            marker = 0
        if (marker < 0 && lower.endsWith("/content/paks"))
            marker = lower.length - "/content/paks".length

        if (marker >= 0) {
            var prior = normalized.lastIndexOf("/", marker - 1)
            return normalized.slice(prior + 1)
        }

        var parts = normalized.split("/")
        var segments = []
        for (var i = 0; i < parts.length; ++i) {
            if (parts[i] !== "")
                segments.push(parts[i])
        }
        if (segments.length <= 2)
            return normalized
        return segments.slice(segments.length - 2).join("/")
    }

    function targetCoreName(file) {
        var name = file && file.name ? String(file.name) : ""
        var dot = name.lastIndexOf(".")
        var base = dot > 0 ? name.slice(0, dot) : name
        if (base.toLowerCase().endsWith("_p"))
            base = base.slice(0, base.length - 2)

        var original = String(section.originalName || "")
        var folder = String(section.folderName || "")
        if (folder.toLowerCase().endsWith("_p"))
            folder = folder.slice(0, folder.length - 2)

        if (original === "" || base.toLowerCase() === original.toLowerCase())
            return folder !== "" ? folder : base
        return base
    }

    readonly property int fileCount: coreFiles.length + companionFiles.length

    readonly property color rootAccent: (isLogicRoot && modManager.theme === "Glassmorphic")
        ? modManager.accentColor
        : window.primaryColor

    implicitHeight: content.implicitHeight + 24
    radius: 11
    color: modManager.theme === "Cozy" ? "#FBF8F4" : Qt.rgba(1, 1, 1, 0.025)
    border.width: 1
    border.color: modManager.theme === "Cozy"
        ? "#EDE4D9"
        : Qt.rgba(rootAccent.r, rootAccent.g, rootAccent.b, 0.18)

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 9

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            Image {
                Layout.preferredWidth: 16
                Layout.preferredHeight: 16
                source: section.isLogicRoot
                    ? "qrc:/UniversalModManager/Lucide Icons/blocks.svg"
                    : (section.isLuaRoot
                        ? "qrc:/UniversalModManager/Lucide Icons/folder-code.svg"
                        : (section.isCppRoot
                            ? "qrc:/UniversalModManager/Lucide Icons/binary.svg"
                            : "qrc:/UniversalModManager/Lucide Icons/folder.svg"))
                sourceSize: Qt.size(16, 16)
                layer.enabled: true
                layer.effect: ColorOverlay {
                    color: section.rootAccent
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 1

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Text {
                        text: section.rootData ? (section.rootData.rootName || "Mods folder") : "Mods folder"
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        color: modManager.theme === "Cozy" ? "#2C2C2C" : "#F1F5F9"
                    }

                    Rectangle {
                        visible: String(section.bundleFolderName || "").trim() !== ""
                        Layout.preferredHeight: 18
                        Layout.preferredWidth: bundleLabel.implicitWidth + 12
                        radius: 9
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.12)
                        border.width: 1
                        border.color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.3)

                        Text {
                            id: bundleLabel
                            anchors.centerIn: parent
                            text: "Bundle"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            color: section.rootAccent
                        }
                    }

                    Rectangle {
                        visible: section.willCreate
                        Layout.preferredHeight: 18
                        Layout.preferredWidth: createdLabel.implicitWidth + 12
                        radius: 9
                        color: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.16)

                        Text {
                            id: createdLabel
                            anchors.centerIn: parent
                            text: "Will be created"
                            font.pixelSize: 9
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Cozy" ? window.primaryColor : "#FFFFFF"
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: section.displayRootPath !== "" ? section.displayRootPath : "Destination unavailable"
                    font.family: "JetBrains Mono"
                    font.pixelSize: 11
                    color: modManager.theme === "Cozy" ? "#6F5F4E" : "#A3B0C4"
                    elide: Text.ElideMiddle
                }
            }

            Rectangle {
                Layout.preferredHeight: 22
                Layout.preferredWidth: countLabel.implicitWidth + 14
                radius: 11
                color: Qt.rgba(section.rootAccent.r, section.rootAccent.g, section.rootAccent.b, 0.12)

                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    text: section.fileCount + (section.fileCount === 1 ? " file" : " files")
                    font.pixelSize: 10
                    font.weight: Font.DemiBold
                    color: section.rootAccent
                }
            }
        }

        Column {
            Layout.fillWidth: true
            spacing: 6
            visible: section.coreFiles.length > 0

            Repeater {
                model: section.coreFiles

                delegate: Item {
                    id: coreFileDelegate
                    width: parent ? parent.width : 0
                    height: coreFileRow.implicitHeight
                    property var fileItem: modelData

                    InstallFileRow {
                        id: coreFileRow
                        width: parent.width
                        fileName: section.targetCoreName(coreFileDelegate.fileItem)
                        fileSize: coreFileDelegate.fileItem.size || ""
                        fileExtension: coreFileDelegate.fileItem.extension || ""
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            visible: section.companionFiles.length > 0
            implicitHeight: companionColumn.implicitHeight + 12
            radius: 8
            color: companionToggle.containsMouse
                ? (modManager.theme === "Cozy" ? Qt.rgba(0, 0, 0, 0.035) : Qt.rgba(1, 1, 1, 0.045))
                : "transparent"

            Column {
                id: companionColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 6
                spacing: 6

                Rectangle {
                    width: parent.width
                    height: 32
                    radius: 7
                    color: "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 2
                        anchors.rightMargin: 2
                        spacing: 8

                        Image {
                            Layout.preferredWidth: 13
                            Layout.preferredHeight: 13
                            source: section.companionsExpanded
                                ? "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
                                : "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                            sourceSize: Qt.size(13, 13)
                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Companion files"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: modManager.theme === "Cozy" ? "#5A4A3A" : "#B8C4D6"
                        }

                        Text {
                            text: section.companionFiles.length
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            color: modManager.theme === "Cozy" ? "#8A7A6A" : "#8291A8"
                        }
                    }

                    MouseArea {
                        id: companionToggle
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: section.companionsExpanded = !section.companionsExpanded
                    }
                }

                Column {
                    width: parent.width
                    spacing: 6
                    visible: section.companionsExpanded

                    Repeater {
                        model: section.companionFiles

                        delegate: Item {
                            id: companionFileDelegate
                            width: parent ? parent.width : 0
                            height: companionFileRow.implicitHeight
                            property var fileItem: modelData

                            InstallFileRow {
                                id: companionFileRow
                                width: parent.width
                                fileName: companionFileDelegate.fileItem.name || ""
                                relativePath: companionFileDelegate.fileItem.relativePath || ""
                                showRelativePath: true
                                fileSize: companionFileDelegate.fileItem.size || ""
                                fileExtension: companionFileDelegate.fileItem.extension || ""
                            }
                        }
                    }
                }
            }
        }
    }
}
