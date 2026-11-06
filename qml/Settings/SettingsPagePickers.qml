import QtQuick
import QtQuick.Dialogs

Item {
    id: settingsPickers
    anchors.fill: parent

    property alias scanTimer: scanTimer
    property alias gamePathDialog: gamePathDialog
    property alias modsPathDialog: modsPathDialog
    property alias downloadsPathDialog: downloadsPathDialog
    property alias backupPathDialog: backupPathDialog
    property alias sevenZipPathDialog: sevenZipPathDialog
    property alias pakViewerPathDialog: pakViewerPathDialog
    property alias conflictDetectorPathDialog: conflictDetectorPathDialog
    property alias ueDetectorPathDialog: ueDetectorPathDialog
    property alias gamePaksPathDialog: gamePaksPathDialog
    property alias usmapFileDialog: usmapFileDialog
    property alias wallpaperDialog: wallpaperDialog
    property alias colorPickerDialog: colorPickerDialog
    property alias navBarColorDialog: navBarColorDialog
    property alias cpuBarColorDialog: cpuBarColorDialog
    property alias ramBarColorDialog: ramBarColorDialog
    property alias driveBarColorDialog: driveBarColorDialog
    property alias navBarIconColorDialog: navBarIconColorDialog

    Timer {
        id: scanTimer
        interval: 5000
        repeat: false
        onTriggered: scanningPopup.close()
    }

    FileDialog {
        id: gamePathDialog
        title: "Select Game Executable"
        nameFilters: ["Executable files (*.exe)"]
        onAccepted: modManager.setGamePath(selectedFile.toString().replace("file:///", ""))
    }

    FolderDialog {
        id: modsPathDialog
        title: "Select Mods Folder"
        onAccepted: modManager.setModsPath(selectedFolder.toString().replace("file:///", ""))
    }

    FolderDialog {
        id: downloadsPathDialog
        title: "Select Downloads Folder"
        onAccepted: modManager.setDownloadsPath(selectedFolder.toString().replace("file:///", ""))
    }

    FolderDialog {
        id: backupPathDialog
        title: "Select Backup Folder"
        onAccepted: modManager.setBackupPath(selectedFolder.toString().replace("file:///", ""))
    }

    FileDialog {
        id: sevenZipPathDialog
        title: "Select 7-Zip or WinRAR"
        nameFilters: ["Executable files (*.exe)"]
        onAccepted: modManager.setSevenZipPath(selectedFile.toString().replace("file:///", ""))
    }

    FileDialog {
        id: pakViewerPathDialog
        title: "Select Assets Viewer Executable"
        nameFilters: ["Executable files (*.exe)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            path = decodeURIComponent(path)
            modManager.setPakViewerPath(path)
        }
    }

    FileDialog {
        id: conflictDetectorPathDialog
        title: "Select Mods Conflict Detector Executable"
        nameFilters: ["Executable files (*.exe)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            path = decodeURIComponent(path)
            modManager.setConflictDetectorPath(path)
        }
    }

    FileDialog {
        id: ueDetectorPathDialog
        title: "Select Unreal Engine Detector Executable"
        nameFilters: ["Executable files (*.exe)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            path = decodeURIComponent(path)
            modManager.setUeDetectorPath(path)
        }
    }

    FolderDialog {
        id: gamePaksPathDialog
        title: "Select Game Paks Folder"
        onAccepted: {
            var path = selectedFolder.toString().replace("file:///", "")
            path = decodeURIComponent(path)
            modManager.setGamePaksPath(path)
        }
    }

    FileDialog {
        id: usmapFileDialog
        title: "Select Usmap File"
        nameFilters: ["Usmap files (*.usmap)", "All files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace("file:///", "")
            path = decodeURIComponent(path)
            modManager.setUsmapFilePath(path)
        }
    }

    FileDialog {
        id: wallpaperDialog
        title: "Select Wallpaper Image"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Image files (*.png *.jpg *.jpeg *.bmp *.gif)", "All files (*.*)"]
        onAccepted: modManager.setWallpaperPath(selectedFile.toString().replace("file:///", ""))
    }

    ColorPicker {
        id: colorPickerDialog
        targetProperty: "idColor"
        currentColor: "#FF6B35"
    }

    ColorPicker {
        id: navBarColorDialog
        targetProperty: "navBarColor"
        currentColor: modManager.navBarColor
    }

    ColorPicker {
        id: cpuBarColorDialog
        targetProperty: "cpuUsageBarColor"
        currentColor: modManager.cpuUsageBarColor
    }

    ColorPicker {
        id: ramBarColorDialog
        targetProperty: "ramUsageBarColor"
        currentColor: modManager.ramUsageBarColor
    }

    ColorPicker {
        id: driveBarColorDialog
        targetProperty: "driveUsageBarColor"
        currentColor: modManager.driveUsageBarColor
    }

    ColorPicker {
        id: navBarIconColorDialog
        targetProperty: "navBarIconColor"
        currentColor: window.navBarIconColor
    }
}
