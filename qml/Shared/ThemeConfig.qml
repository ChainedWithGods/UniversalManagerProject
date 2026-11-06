import QtQuick

QtObject {
    id: themeConfig

    readonly property var cozy: ({
        bgColor: '#FFFAF5',
        surfaceColor: '#FFFAF5',
        primaryColor: '#A47A51',
        accentColor: '#A47A51',
        cardColor1: "#FFFAF5",
        cardColor2: "#FFFAF5",
        cardColor3: "#FFFAF5",
        cardColor4: "#FFFAF5",
        textColor: "#000000",
        textSecondaryColor: "#000000",
        borderColor: '#b3947d',
        borderWidth: 1,
        titleBarColor: "#F5ECE2",
        secondaryButtonColor: "transparent",
        secondaryButtonHoverColor: Qt.rgba(164 / 255, 122 / 255, 81 / 255, 0.10),
        secondaryButtonBorderColor: "#B3947D",
        secondaryButtonTextColor: "#3E3E3E",
        secondaryButtonTextHoverColor: "#000000",
        separatorColor: Qt.rgba(164 / 255, 122 / 255, 81 / 255, 0.22),
        cardRadius: 15,
        shadowEnabled: true,
        tagColor: "#8A623C",
        enableButtonColor: "#619e80",
        disableButtonColor: "#4e4e5a",
        idColor: "#966C6C",
        folderModColor: "#6E7A5C",
        looseFileColor: "#B5645A",
        pakColor: "#B26A3C",
        ucasColor: "#A0525E",
        utocColor: "#8A7A45",
        cpuUsageBarColor: "#C08A3E",
        ramUsageBarColor: "#B5645A",
        driveUsageBarColor: "#6E7A5C",

        generatingGradientTop: '#000000',
        generatingGradientBottom: '#ff9137',

        windowGlowGradientTop: '#fcc884',
        windowGlowGradientBottom: '#cf881e',

        contextMenuGradientTop: '#fffdfb',
        contextMenuGradientBottom: '#ffeadd',

        dialogGradientTop: "#FFFAF5",
        dialogGradientBottom: '#ffeedf',

        navBarGradientTop: "#FFFCF7",
        navBarGradientBottom: '#ffeedf',

        toastGradientTop: "#FFFAF5",
        toastGradientBottom: "#FFE5D0",
        toastBorderColor: "#E8C4A0",
        toastTextColor: "#2C2C2C",
        toastIconColor: "#D6A963",
        toastAccentColor: "#D6A963",

    })

    readonly property var wavey: ({
        bgColor: "transparent",
        surfaceColor: Qt.rgba(0.12, 0.24, 0.37, 0.85),
        primaryColor: "#3b82f6",
        accentColor: "#60a5fa",
        textColor: "#ffffff",
        textSecondaryColor: "#e0f2fe",
        borderColor: Qt.rgba(0.59, 0.65, 0.98, 0.3),
        borderWidth: 1.5,
        titleBarColor: "#003863",
        secondaryButtonColor: Qt.rgba(1, 1, 1, 0.08),
        secondaryButtonHoverColor: Qt.rgba(1, 1, 1, 0.15),
        secondaryButtonBorderColor: Qt.rgba(1, 1, 1, 0.12),
        secondaryButtonTextColor: Qt.rgba(1, 1, 1, 0.75),
        secondaryButtonTextHoverColor: "#FFFFFF",
        separatorColor: Qt.rgba(1, 1, 1, 0.10),
        cardRadius: 15,
        shadowEnabled: true,
        tagColor: "#0284C7",
        enableButtonColor: "#3b82f6",
        disableButtonColor: "#334155",
        idColor: "#7DD3FC",
        folderModColor: "#0E7490",
        looseFileColor: "#2563EB",
        pakColor: "#38BDF8",
        ucasColor: "#22D3EE",
        utocColor: "#818CF8",
        cpuUsageBarColor: "#3B82F6",
        ramUsageBarColor: "#818CF8",
        driveUsageBarColor: "#22D3EE",
        iconColor: "#3b82f6",
        glowColor: "#3b82f6",
        cardColor1: Qt.rgba(0.12, 0.23, 0.37, 0.5),
        cardColor2: Qt.rgba(0.23, 0.37, 0.53, 0.5),
        cardColor3: Qt.rgba(0.12, 0.23, 0.37, 0.3),
        cardColor4: Qt.rgba(0.23, 0.37, 0.53, 0.3),

        generatingGradientTop: '#addeff',
        generatingGradientBottom: '#6932ff',

        windowGlowGradientTop: '#addeff',
        windowGlowGradientBottom: '#269cfd',

        navBarGradientTop: '#0061bb',
        navBarGradientBottom: '#000000',

        dialogGradientTop: '#124180',
        dialogGradientBottom: '#040f29',

        toastGradientTop: "#1e3a8a",
        toastGradientBottom: "#0c1e3f",
        toastBorderColor: Qt.rgba(59, 130, 246, 0.4),
        toastTextColor: "#e0f2fe",
        toastIconColor: "#3b82f6",
        toastAccentColor: "#60a5fa",

        useAcrylicBlur: true,
        acrylicOpacity: 0.50,
        acrylicTintColor: "#5e44f7",
        acrylicBlurAmount: 0.50,
        backgroundBlurAmount: 0.80,

        labelModsInstalled: "MODS INSTALLED",
        labelModsInstalledColor: "#e0f2fe",
        cardModsInstalledColor: "#1e3a5f",
        cardModsInstalledOpacity: 1,

        labelActiveMods: "ACTIVE MODS",
        labelActiveModsColor: "#e0f2fe",
        cardActiveModsColor: "#1e3a5f",
        cardActiveModsOpacity: 1,

        labelInactiveMods: "INACTIVE MODS",
        labelInactiveModsColor: "#e0f2fe",
        cardInactiveModsColor: "#1e3a5f",
        cardInactiveModsOpacity: 1,

        labelStorageUsed: "STORAGE USED",
        labelStorageUsedColor: "#e0f2fe",
        cardStorageUsedColor: "#1e3a5f",
        cardStorageUsedOpacity: 1,

        labelQuickActions: "Quick Actions",
        labelQuickActionsColor: "#e0f2fe",

        labelInstallMod: "Install Mod",
        labelInstallModColor: "#e0f2fe",
        cardInstallModColor: "#1e3a5f",
        cardInstallModOpacity: 1,

        labelBrowseWorkshop: "Browse Workshop",
        labelBrowseWorkshopColor: "#e0f2fe",
        cardBrowseWorkshopColor: "#1e3a5f",
        cardBrowseWorkshopOpacity: 1,

        homeGreetingColor: "#e0f2fe",
        homeModGalleryColor: "#e0f2fe",
        homeNoImagesColor: "#93c5fd",
        homeNoImagesSubColor: "#93c5fd",

        settingsTitleColor: "#e0f2fe",
        settingsSubtitleColor: "#bae6fd",
    })

    readonly property var glassmorphic: ({
        bgColor: "transparent",
        surfaceColor: Qt.rgba(1, 1, 1, 0.25),
        textColor: "#FFFFFF",
        textSecondaryColor: Qt.rgba(1, 1, 1, 0.8),
        borderColor: Qt.rgba(1, 1, 1, 0.4),
        borderWidth: 1,
        titleBarColor: null,
        secondaryButtonColor: Qt.rgba(1, 1, 1, 0.08),
        secondaryButtonHoverColor: Qt.rgba(1, 1, 1, 0.15),
        secondaryButtonBorderColor: Qt.rgba(1, 1, 1, 0.12),
        secondaryButtonTextColor: Qt.rgba(1, 1, 1, 0.75),
        secondaryButtonTextHoverColor: "#FFFFFF",
        separatorColor: Qt.rgba(1, 1, 1, 0.10),
        cardRadius: 15,
        shadowEnabled: true,

        generatingGradientTop: null,
        generatingGradientBottom: null,

        windowGlowGradientTop: null,
        windowGlowGradientBottom: null,

        toastGradientTop: Qt.rgba(0.12, 0.12, 0.15, 0.95),
        toastGradientBottom: Qt.rgba(0.08, 0.08, 0.12, 0.95),
        toastBorderColor: Qt.rgba(1, 1, 1, 0.25),
        toastTextColor: "#FFFFFF",
        toastIconColor: null,
        toastAccentColor: null,

        navBarGradientTop: Qt.rgba(1, 1, 1, 0.18),
        navBarGradientBottom: Qt.rgba(1, 1, 1, 0.10),
    })

    readonly property var crimson: ({
        bgColor: "#100000",
        surfaceColor: "#100000",
        primaryColor: "#aa2446",
        accentColor: "#c34a4a",
        textColor: "#ffffff",
        textSecondaryColor: "#ffffff",
        borderColor: "#411818",
        borderWidth: 1,
        titleBarColor: "#420000",
        secondaryButtonColor: Qt.rgba(1, 1, 1, 0.08),
        secondaryButtonHoverColor: Qt.rgba(1, 1, 1, 0.15),
        secondaryButtonBorderColor: Qt.rgba(1, 1, 1, 0.12),
        secondaryButtonTextColor: Qt.rgba(1, 1, 1, 0.75),
        secondaryButtonTextHoverColor: "#FFFFFF",
        separatorColor: Qt.rgba(1, 1, 1, 0.10),
        cardRadius: 15,
        shadowEnabled: true,
        tagColor: "#A32844",
        enableButtonColor: "#338333",
        disableButtonColor: "#716060",
        idColor: "#D08A8A",
        folderModColor: "#8E3E56",
        looseFileColor: "#B84A66",
        pakColor: "#D9774F",
        ucasColor: "#C75B7A",
        utocColor: "#B47A9E",
        cpuUsageBarColor: "#BE6666",
        ramUsageBarColor: "#C75B7A",
        driveUsageBarColor: "#B47A9E",
        iconColor: "#aa2446",
        archiveIconColor: '#B686DF',
        backgroundBlur: 1,
        backgroundBlurMax: 50,
        listHeaderColor: "#5f1414",
        cardColor1: Qt.rgba(0.32, 0.11, 0.27, 0.35),
        cardColor2: Qt.rgba(0.32, 0.11, 0.27, 0.22),
        cardColor3: Qt.rgba(0.32, 0.11, 0.27, 0.22),
        cardColor4: Qt.rgba(0.32, 0.11, 0.27, 0.14),

        generatingGradientTop: "#ad0056",
        generatingGradientBottom: '#6b0000',

        windowGlowGradientTop: "#ff1346",
        windowGlowGradientBottom: "#ffd8dd",

        cardGradientTop: '#120000',
        cardGradientBottom: "#180808",

        themeCardGradientTop: "#120000",
        themeCardGradientBottom: "#180808",

        dialogGradientTop: '#120000',
        dialogGradientBottom: '#180808',

        toastGradientTop: "#120000",
        toastGradientBottom: "#180808",
        toastBorderColor: "#5A124F",
        toastTextColor: "#ffffff",
        toastIconColor: "#A78BFA",
        toastAccentColor: "#521D45",

        sidebarButtonGradientTop: "#120000",
        sidebarButtonGradientBottom: "#180808",

        navBarGradientTop: '#120000',
        navBarGradientBottom: '#180808',
    })

    function applyTheme(themeName, window, accentColorFromSettings) {
        var palette
        if (themeName === "Glassmorphic") {
            palette = glassmorphic
        } else if (themeName === "Crimson") {
            palette = crimson
        } else if (themeName === "Wavey") {
            palette = wavey
        } else {
            palette = cozy
        }

        if (themeName === "Glassmorphic") {
            window.bgColor = palette.bgColor
            window.primaryColor = accentColorFromSettings
            window.accentColor = Qt.lighter(accentColorFromSettings, 1.3)
            window.iconColor = accentColorFromSettings
        } else if (themeName === "Crimson") {
            window.bgColor = palette.bgColor
            window.primaryColor = palette.primaryColor
            window.accentColor = palette.accentColor
            window.iconColor = palette.iconColor
        } else if (themeName === "Wavey") {
            window.bgColor = palette.bgColor
            window.primaryColor = palette.primaryColor
            window.accentColor = palette.accentColor
            window.iconColor = palette.iconColor
        } else {
            window.bgColor = palette.bgColor
            window.primaryColor = palette.primaryColor
            window.accentColor = palette.accentColor
            window.iconColor = palette.primaryColor
        }

        if (palette.cardColor1) {
            window.cardColor1 = palette.cardColor1
        }
        if (palette.cardColor2) {
            window.cardColor2 = palette.cardColor2
        }
        if (palette.cardColor3) {
            window.cardColor3 = palette.cardColor3
        }
        if (palette.cardColor4) {
            window.cardColor4 = palette.cardColor4
        }

        window.textColor = palette.textColor
        window.textSecondaryColor = palette.textSecondaryColor
        window.borderColor = palette.borderColor
        window.borderWidth = palette.borderWidth
        window.cardRadius = palette.cardRadius
        window.shadowEnabled = palette.shadowEnabled

        if (palette.tagColor) {
            modManager.setTagColor(palette.tagColor)
        }
        if (palette.enableButtonColor) {
            modManager.setEnableButtonColor(palette.enableButtonColor)
        }
        if (palette.disableButtonColor) {
            modManager.setDisableButtonColor(palette.disableButtonColor)
        }
        if (palette.idColor) {
            modManager.setIdColor(palette.idColor)
        }
        if (palette.folderModColor) {
            modManager.setFolderModColor(palette.folderModColor)
        }
        if (palette.looseFileColor) {
            modManager.setLooseFileColor(palette.looseFileColor)
        }
        if (palette.pakColor) {
            modManager.setPakColor(palette.pakColor)
        }
        if (palette.ucasColor) {
            modManager.setUcasColor(palette.ucasColor)
        }
        if (palette.utocColor) {
            modManager.setUtocColor(palette.utocColor)
        }
        if (palette.cpuUsageBarColor) {
            modManager.setCpuUsageBarColor(palette.cpuUsageBarColor)
        }
        if (palette.ramUsageBarColor) {
            modManager.setRamUsageBarColor(palette.ramUsageBarColor)
        }
        if (palette.driveUsageBarColor) {
            modManager.setDriveUsageBarColor(palette.driveUsageBarColor)
        }

        var topProp = themeName === "Cozy" ? "cozyDialogGradientTop"
                    : themeName === "Crimson" ? "crimsonDialogGradientTop"
                    : themeName === "Wavey" ? "waveyDialogGradientTop"
                    : null
        var botProp = themeName === "Cozy" ? "cozyDialogGradientBottom"
                    : themeName === "Crimson" ? "crimsonDialogGradientBottom"
                    : themeName === "Wavey" ? "waveyDialogGradientBottom"
                    : null
        if (topProp && palette.dialogGradientTop) {
            window[topProp] = palette.dialogGradientTop
        }
        if (botProp && palette.dialogGradientBottom) {
            window[botProp] = palette.dialogGradientBottom
        }
    }
}
