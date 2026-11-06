import QtQuick

Item {
    id: libraryShortcuts
    anchors.fill: parent

    Shortcut {
        sequence: window.shortcutInstall
        onActivated: installModDialog.open()
    }

    Shortcut {
        sequence: window.shortcutBackup
        onActivated: backupTypeDialog.open()
    }

    Shortcut {
        sequence: window.shortcutRefresh
        onActivated: {
            if (modManager.modsPath) {
                modManager.scanMods(modManager.modsPath)
            }
        }
    }

    Shortcut {
        sequence: window.shortcutSearch
        enabled: pageStack.currentIndex === 1
        onActivated: {

            if (modSearchField) {
                modSearchField.forceActiveFocus()
            }
        }
    }

    Shortcut {
        sequence: window.shortcutEnableAll
        onActivated: {
            var allMods = []
            for (var i = 0; i < filteredModList.count; i++) {
                allMods.push(filteredModList.get(i).name)
            }
            if (allMods.length > 0) {
                modManager.toggleModStatusBatch(allMods, true)
            }
        }
    }

    Shortcut {
        sequence: window.shortcutDisableAll
        onActivated: {
            var allMods = []
            for (var i = 0; i < filteredModList.count; i++) {
                allMods.push(filteredModList.get(i).name)
            }
            if (allMods.length > 0) {
                modManager.toggleModStatusBatch(allMods, false)
            }
        }
    }

    Shortcut {
        sequence: window.shortcutDelete
        enabled: viewMode === "details" && selectedMods.length > 0
        onActivated: {
            if (selectedMods.length === 1) {
                modManager.deleteMod(selectedMods[0])
            }
        }
    }

    Shortcut {
        sequence: window.shortcutToggleMod
        enabled: hoveredModName !== ""
        onActivated: {
            if (hoveredModName !== "") {
                modManager.toggleModStatus(hoveredModName)
            }
        }
    }

    Shortcut {
        sequence: window.shortcutOpenFolder
        enabled: hoveredModName !== ""
        context: Qt.ApplicationShortcut
        onActivated: {
            console.log("Open Folder shortcut triggered for:", hoveredModName)
            if (hoveredModName !== "") {
                modManager.openModFolder(hoveredModName)
            }
        }
    }

    Shortcut {
        sequence: window.shortcutShowInPakViewer
        enabled: hoveredModName !== ""
        context: Qt.ApplicationShortcut
        onActivated: {
            if (hoveredModName !== "") {
                var pakPath = modManager.getModPakPath(hoveredModName)
                if (pakPath && pakPath !== "") {
                    modManager.launchPakViewer(pakPath)
                    toastNotification.show("Launching PakViewer...", "scan.svg")
                } else {
                    toastNotification.show("No .pak file found for this mod", "file-x.svg")
                }
            }
        }
    }

    Shortcut {
        sequence: window.shortcutExportModStructure
        enabled: hoveredModName !== ""
        context: Qt.ApplicationShortcut
        onActivated: {
            if (hoveredModName !== "") {
                var missing = []
                if (!modManager.gamePaksPath || modManager.gamePaksPath === "") {
                    missing.push("Game Paks folder")
                }
                if (!modManager.usmapFilePath || modManager.usmapFilePath === "") {
                    missing.push("USMAP file")
                }

                if (missing.length > 0) {
                    var msg = "Set " + missing.join(" and ") + " in Settings before exporting"
                    toastNotification.show(msg, "circle-x.svg")
                    return
                }

                modManager.exportModStructure(hoveredModName)
                toastNotification.show("Exporting mod structure...", "share.svg")
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+A"
        enabled: viewMode === "details"
        onActivated: {
            var allMods = []
            for (var i = 0; i < filteredModList.count; i++) {
                allMods.push(filteredModList.get(i).name)
            }
            selectedMods = allMods
        }
    }

}
