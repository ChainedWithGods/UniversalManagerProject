import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    property int allCount: 0
    property int unfinishedCount: 0
    property int finishedCount: 0
    property int archiveCount: 0
    property int modFileCount: 0
    property int modUpdatesCount: 0
    property int downloadedModsCount: 0
    property string category: "all"
    signal categorySelected(string key)

    radius: 0
    topLeftRadius: window.cardRadius
    bottomLeftRadius: window.cardRadius
    color: modManager.theme === "Cozy"
        ? Qt.rgba(0, 0, 0, 0.025)
        : (modManager.theme === "Glassmorphic"
            ? Qt.rgba(window.surfaceColor.r, window.surfaceColor.g, window.surfaceColor.b,
                      Math.min(1, modManager.glassOpacity * 0.3 + modManager.glassDarkness * 0.2))
            : Qt.rgba(1, 1, 1, 0.035))

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 18
        anchors.bottomMargin: 12
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 3

        Text {
            Layout.leftMargin: 10
            Layout.bottomMargin: 6
            text: "LIBRARY"
            font.pixelSize: 9
            font.weight: Font.DemiBold
            font.letterSpacing: 1.2
            color: window.textSecondaryColor
            opacity: 0.75
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "All downloads"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/layers.svg"
            count: root.allCount
            selected: root.category === "all"
            onClicked: root.categorySelected("all")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Unfinished"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/circle-play.svg"
            count: root.unfinishedCount
            selected: root.category === "unfinished"
            onClicked: root.categorySelected("unfinished")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Finished"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/circle-check.svg"
            count: root.finishedCount
            selected: root.category === "finished"
            onClicked: root.categorySelected("finished")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Mod Updates"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/cloud-download.svg"
            count: root.modUpdatesCount
            selected: root.category === "modupdates"
            onClicked: root.categorySelected("modupdates")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Downloaded Mods"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/package.svg"
            count: root.downloadedModsCount
            selected: root.category === "moddownloads"
            onClicked: root.categorySelected("moddownloads")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Archives"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/archive.svg"
            count: root.archiveCount
            selected: root.category === "archives"
            onClicked: root.categorySelected("archives")
        }

        DownloadsSidebarItem {
            Layout.fillWidth: true
            label: "Loose files"
            iconSource: "qrc:/UniversalModManager/Lucide Icons/file-box.svg"
            count: root.modFileCount
            selected: root.category === "modfiles"
            onClicked: root.categorySelected("modfiles")
        }

        Item { Layout.fillHeight: true }
    }

    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.12)
        visible: modManager.theme === "Glassmorphic"
    }
}
