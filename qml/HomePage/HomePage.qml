import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

Pane {
    id: homePage
    padding: 30
    leftPadding: 60
    rightPadding: 60
    background: Rectangle { color: modManager.theme === "Crimson" ? "transparent" : window.bgColor }
    property int descriptionRefreshTrigger: 0
    property color waveyBadgeColor: Qt.lighter(window.surfaceColor, 1.12)
    property color statsBadgeColor: modManager.theme === "Wavey"
        ? waveyBadgeColor
        : solidTint(window.cardColor1)
    function solidTint(tintColor) {
        var t = tintColor.a
        if (t < 0) t = 0
        if (t > 1) t = 1
        return Qt.rgba(
            window.surfaceColor.r * (1 - t) + tintColor.r * t,
            window.surfaceColor.g * (1 - t) + tintColor.g * t,
            window.surfaceColor.b * (1 - t) + tintColor.b * t,
            1
        )
    }
    readonly property alias currentHint: homePageTips.currentHint

    ColumnLayout {
        anchors.fill: parent
        spacing: 30
        clip: false

        Column {
            Layout.fillWidth: true
            spacing: 5

            Item {
                width: greetingTextHidden.width
                height: greetingTextHidden.height

                Text {
                    id: greetingTextHidden
                    text: modManager.greeting + "!"
                    font.pixelSize: 32
                    font.weight: 800
                    visible: false
                }

                Text {
                    text: greetingTextHidden.text
                    font: greetingTextHidden.font
                    color: window.textColor
                    opacity: 0.3
                }

                Rectangle {
                    id: shimmerRect
                    anchors.fill: parent
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: shimmerRect.shimmerPos - 0.2; color: "transparent" }
                        GradientStop { position: shimmerRect.shimmerPos - 0.05; color: shimmerRect.waveColorA }
                        GradientStop { position: shimmerRect.shimmerPos + 0.05; color: shimmerRect.waveColorB }
                        GradientStop { position: shimmerRect.shimmerPos + 0.2; color: "transparent" }
                    }
                    layer.enabled: true
                    layer.effect: OpacityMask {
                        maskSource: greetingTextHidden
                    }

                    property real shimmerPos: 0
                    property color waveColorA: modManager.theme === "Wavey"
                        ? "#1b3185"
                        : (modManager.theme === "Glassmorphic"
                           ? modManager.accentColor
                           : (modManager.theme === "Crimson"
                              ? window.accentColor
                              : (modManager.theme === "Cozy"
                                 ? window.accentColor
                                 : window.primaryColor)))
                    property color waveColorB: modManager.theme === "Wavey"
                        ? "#0a2742"
                        : waveColorA
                    NumberAnimation on shimmerPos {
                        from: -0.5
                        to: 1.5
                        duration: window.getAnimationDuration(4500)
                        loops: Animation.Infinite
                        running: !window.animationsPaused
                    }
                }

                layer.enabled: true
                layer.effect: MultiEffect {
                    shadowEnabled: true
                    shadowColor: modManager.theme === "Crimson" ? window.accentColor : (modManager.theme === "Wavey" ? window.accentColor : (modManager.theme === "Cozy" ? window.accentColor : window.primaryColor))
                    shadowOpacity: modManager.theme === "Crimson" ? 1.0 : (modManager.theme === "Wavey" ? 0.8 : (modManager.theme === "Cozy" ? 0.6 : 0.5))
                    shadowBlur: modManager.theme === "Crimson" ? 1.0 : (modManager.theme === "Wavey" ? 0.8 : (modManager.theme === "Cozy" ? 0.5 : 0.6))
                    shadowVerticalOffset: 0
                    shadowHorizontalOffset: 0
                }
            }

            HomePageTips {
                id: homePageTips
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            StatCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                title: "Mods Installed"
                value: modManager.modsInstalled
                iconSource: modManager.theme === "Wavey"
                            ? "qrc:/UniversalModManager/Icons/Wavey Theme/Home page/Installed Mods.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/Home page/Installed mods.png"
                               : (modManager.theme === "Glassmorphic"
                                  ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Home page/Mods installed.png"
                                  : (modManager.theme === "Cozy"
                                     ? "qrc:/UniversalModManager/Icons/Cozy Theme/Home page/Installed Mods.png"
                                     : "qrc:/UniversalModManager/Icons/Cozy Theme/mods icon.png")))
                backgroundColor: homePage.statsBadgeColor
                iconBgColor: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                iconColor: window.primaryColor
                isHorizontal: true
            }

            StatCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                title: "Active Mods"
                value: modManager.modsActive
                iconSource: modManager.theme === "Wavey"
                            ? "qrc:/UniversalModManager/Icons/Wavey Theme/Home page/active mods.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/Home page/Active Mod.png"
                               : (modManager.theme === "Glassmorphic"
                                  ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Home page/Active mods.png"
                                  : (modManager.theme === "Cozy"
                                     ? "qrc:/UniversalModManager/Icons/Cozy Theme/Home page/active mods.png"
                                     : "qrc:/UniversalModManager/Icons/Cozy Theme/active Mods.png")))
                backgroundColor: homePage.statsBadgeColor
                iconBgColor: Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                iconColor: window.accentColor
                isHorizontal: true
            }

            StatCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                title: "Inactive Mods"
                value: modManager.modsInstalled - modManager.modsActive
                iconSource: modManager.theme === "Wavey"
                            ? "qrc:/UniversalModManager/Icons/Wavey Theme/Home page/inactive mods.png"
                            : (modManager.theme === "Crimson"
                               ? "qrc:/UniversalModManager/Icons/Crimson Theme/Home page/Inactive mods.png"
                               : (modManager.theme === "Glassmorphic"
                                  ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Home page/Inactive Mods.png"
                                  : (modManager.theme === "Cozy"
                                     ? "qrc:/UniversalModManager/Icons/Cozy Theme/Home page/inactive mods.png"
                                     : "qrc:/UniversalModManager/Icons/Cozy Theme/inactive mods.png")))
                backgroundColor: homePage.statsBadgeColor
                iconBgColor: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
                iconColor: window.primaryColor
                isHorizontal: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: modManager.theme === "Crimson" ? "transparent" :
                       (modManager.theme === "Glassmorphic"
                    ? window.surfaceColor
                    : homePage.statsBadgeColor)
                Behavior on color { ColorAnimation { duration: 300 } }
                radius: window.cardRadius
                Behavior on radius { NumberAnimation { duration: 300 } }
                border.color: modManager.theme === "Glassmorphic" ? window.borderColor : "transparent"
                Behavior on border.color { ColorAnimation { duration: 300 } }
                border.width: modManager.theme === "Glassmorphic" ? 1 : 0
                Behavior on border.width { NumberAnimation { duration: 300 } }

                gradient: modManager.theme === "Crimson" ? storageCardGradient : null

                Gradient {
                    id: storageCardGradient
                    GradientStop { position: 0.0; color: window.crimsonGradientTop }
                    GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                }

                layer.enabled: modManager.theme !== "Glassmorphic"
                layer.effect: MultiEffect {
                    shadowEnabled: modManager.theme !== "Glassmorphic"
                    shadowOpacity: 0.03
                    shadowBlur: 0.1
                    shadowVerticalOffset: 4
                    shadowHorizontalOffset: 0
                    shadowColor: "#000000"
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 20

                    Item {
                        Layout.preferredWidth: 40
                        Layout.preferredHeight: 40
                        Layout.alignment: Qt.AlignVCenter

                        Image {
                            id: storageIcon
                            anchors.fill: parent
                            source: modManager.theme === "Wavey"
                                    ? "qrc:/UniversalModManager/Icons/Wavey Theme/Home page/storage used.png"
                                    : (modManager.theme === "Crimson"
                                       ? "qrc:/UniversalModManager/Icons/Crimson Theme/Home page/Storage used.png"
                                       : (modManager.theme === "Glassmorphic"
                                          ? "qrc:/UniversalModManager/Icons/Glassmorphic Theme/Home page/storage used.png"
                                          : "qrc:/UniversalModManager/Icons/Cozy Theme/Home page/storage used.png"))
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            mipmap: true
                            cache: false
                        }

                        ColorOverlay {
                            anchors.fill: storageIcon
                            source: storageIcon
                            color: window.accentColor
                            visible: modManager.theme === "Glassmorphic"
                        }
                    }

                    Column {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        spacing: 2

                        RowLayout {
                            width: parent.width

                            Text {
                                text: modManager.storageUsed || "0 B"
                                font.pixelSize: 12
                                font.weight: 600
                                color: window.textColor
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }

                            Item { Layout.fillWidth: true }

                            Text {
                                text: "Storage Used"
                                font.pixelSize: 12
                                font.weight: 600
                                color: window.textSecondaryColor
                                Behavior on color { ColorAnimation { duration: 300 } }
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 8
                            radius: 4
                            color: Qt.rgba(window.textSecondaryColor.r, window.textSecondaryColor.g, window.textSecondaryColor.b, 0.2)

                            Rectangle {
                                width: {

                                    var modsSize = modManager.storageUsed || "0 B"
                                    if (modsSize === "0 B") {
                                        return 0
                                    }

                                    var sizeStr = modsSize.toLowerCase()
                                    if (sizeStr.includes("kb") || sizeStr.includes("b")) {
                                        return parent.width * 0.05
                                    } else if (sizeStr.includes("mb")) {
                                        var mbValue = parseFloat(sizeStr.replace("mb", "").trim())
                                        if (mbValue < 500) return parent.width * 0.08
                                        else return parent.width * 0.15
                                    } else if (sizeStr.includes("gb")) {
                                        var gbValue = parseFloat(sizeStr.replace("gb", "").trim())
                                        if (gbValue < 1) return parent.width * 0.10
                                        else if (gbValue < 5) return parent.width * 0.30
                                        else if (gbValue < 10) return parent.width * 0.50
                                        else if (gbValue < 20) return parent.width * 0.70
                                        else return parent.width * 0.90
                                    }
                                    return parent.width * 0.25
                                }
                                height: parent.height
                                radius: 4
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: window.primaryColor }
                                    GradientStop { position: 1.0; color: window.accentColor }
                                }
                                Behavior on width { NumberAnimation { duration: 400; easing.type: Easing.OutCubic } }
                            }
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: 400
            spacing: 30

            Item {
                id: gallerySlot
                Layout.fillWidth: true
                Layout.preferredWidth: 800
                Layout.fillHeight: true

                Rectangle {
                    id: gallerySlideshow

                    width: Math.min(gallerySlot.width, gallerySlot.height * animatedAspect)
                    height: width / animatedAspect
                    anchors.centerIn: parent

                    color: modImages.length > 0 ? "#000000" : "transparent"
                    radius: 0
                    clip: true

                    layer.enabled: false

                    property int currentIndex: 0
                    property int lastIndex: 0
                    property var modImages: []
                    property bool autoPlay: true
                    property int transitionDuration: 750
                    property real currentOpacity: 1.0
                    property real incomingStartScale: 1.04
                    property real outgoingEndScale: 1.08
                    property real slideOffset: 26
                    property int transitionDirection: 1
                    property int displayDurationMs: 4500
                    property real displayJitter: 0
                    property bool transitioning: false
                    property int imageReloadToken: 0

                    property var aspectCache: ({})
                    property int aspectRevision: 0
                    readonly property string currentImagePath: (modImages.length > 0 && currentIndex < modImages.length)
                        ? modImages[currentIndex].path : ""
                    readonly property string nextImagePath: (modImages.length > 1)
                        ? modImages[(currentIndex + 1) % modImages.length].path : ""
                    readonly property real targetAspect: {
                        var _ = aspectRevision
                        if (currentImagePath && aspectCache[currentImagePath] > 0)
                            return aspectCache[currentImagePath]
                        if (currentStaticImage.implicitWidth > 0 && currentStaticImage.implicitHeight > 0)
                            return currentStaticImage.implicitWidth / currentStaticImage.implicitHeight
                        if (currentGifImage.implicitWidth > 0 && currentGifImage.implicitHeight > 0)
                            return currentGifImage.implicitWidth / currentGifImage.implicitHeight
                        return 16 / 9
                    }
                    property real animatedAspect: targetAspect
                    Behavior on animatedAspect {
                        NumberAnimation { duration: 400; easing.type: Easing.OutCubic }
                    }
                    function cacheAspect(path, w, h) {
                        if (!path || w <= 0 || h <= 0)
                            return
                        aspectCache[path] = w / h
                        aspectRevision++
                    }

                    Component.onCompleted: loadModImages()

                    Connections {
                        target: modManager
                        function onModListChanged() {
                            gallerySlideshow.loadModImages()
                        }
                        function onModImageChanged() {
                            gallerySlideshow.imageReloadToken++
                            gallerySlideshow.loadModImages()
                        }
                    }

                    function buildImageSource(imagePath) {
                        if (!imagePath || imagePath === "")
                            return ""
                        var separator = imagePath.indexOf("?") === -1 ? "?" : "&"
                        return imagePath + separator + "v=" + imageReloadToken
                    }

                    function loadModImages() {
                        var images = []
                        var modList = modManager.modList

                        for (var i = 0; i < modList.length; i++) {
                            var mod = modList[i]
                            var imageKey = mod.type === "loose" && mod.uniqueId ? mod.uniqueId : mod.name
                            var imagePath = modManager.getModImage(imageKey)

                            if (imagePath && imagePath !== "") {
                                images.push({
                                    path: buildImageSource(imagePath),
                                    name: mod.name,
                                    isGif: imagePath.toLowerCase().endsWith(".gif")
                                })
                            }
                        }

                        modImages = images
                        if (currentIndex >= images.length) {
                            currentIndex = 0
                        }
                        lastIndex = currentIndex
                        transitionStaticImage.source = ""
                        transitionGifImage.source = ""
                        transitionStaticImage.opacity = 0
                        transitionGifImage.opacity = 0
                        currentStaticImage.opacity = currentOpacity
                        currentGifImage.opacity = currentOpacity
                        currentStaticImage.scale = 1.0
                        currentGifImage.scale = 1.0
                        transitionStaticImage.scale = 1.0
                        transitionGifImage.scale = 1.0
                        currentStaticImage.x = 0
                        currentGifImage.x = 0
                        transitionStaticImage.x = 0
                        transitionGifImage.x = 0

                        scheduleNext()
                    }

                    function nextImage() {
                        if (transitioning || modImages.length === 0) return
                        transitionDirection = 1
                        cancelNext()
                        currentIndex = (currentIndex + 1) % modImages.length
                    }

                    function previousImage() {
                        if (transitioning || modImages.length === 0) return
                        transitionDirection = -1
                        cancelNext()
                        currentIndex = currentIndex > 0 ? currentIndex - 1 : modImages.length - 1
                    }

                    function scheduleNext() {
                        if (!autoPlay || modImages.length === 0) {
                            slideTimer.stop()
                            return
                        }
                        if (currentIndex >= modImages.length) {
                            currentIndex = 0
                        }
                        var base = displayDurationMs
                        var img = modImages[currentIndex]
                        var typeFactor = (img && img.isGif) ? 1.25 : 1.0
                        var jitterFactor = 1.0 + ((Math.random() * 2 - 1) * displayJitter)
                        slideTimer.interval = Math.max(1200, Math.round(base * typeFactor * jitterFactor))
                        slideTimer.restart()
                    }

                    function cancelNext() {
                        slideTimer.stop()
                    }

                    function startTransition(previousImage) {
                        if (!previousImage) return
                        transitioning = true
                        if (previousImage.isGif) {
                            transitionGifImage.source = previousImage.path
                            transitionStaticImage.source = ""
                            transitionGifImage.opacity = 1
                            transitionStaticImage.opacity = 0
                            transitionGifImage.scale = 1.0
                        } else {
                            transitionStaticImage.source = previousImage.path
                            transitionGifImage.source = ""
                            transitionStaticImage.opacity = 1
                            transitionGifImage.opacity = 0
                            transitionStaticImage.scale = 1.0
                        }
                        currentStaticImage.opacity = 0
                        currentGifImage.opacity = 0
                        currentStaticImage.scale = incomingStartScale
                        currentGifImage.scale = incomingStartScale
                        currentStaticImage.x = slideOffset * transitionDirection
                        currentGifImage.x = slideOffset * transitionDirection
                        transitionStaticImage.x = 0
                        transitionGifImage.x = 0

                        transitionAnim.restart()
                    }

                    Timer {
                        id: slideTimer
                        interval: 0
                        repeat: false
                        running: false
                        onTriggered: gallerySlideshow.nextImage()
                    }

                    onCurrentIndexChanged: {
                        if (modImages.length === 0) return
                        if (currentIndex >= modImages.length) {
                            currentIndex = 0
                            return
                        }
                        var prevIndex = lastIndex
                        lastIndex = currentIndex
                        if (prevIndex === currentIndex) return
                        if (prevIndex < 0 || prevIndex >= modImages.length) return
                        transitionDirection = currentIndex >= prevIndex ? 1 : -1
                        startTransition(modImages[prevIndex])
                    }

                    Image {
                        id: aspectProbeCurrent
                        property string probePath: gallerySlideshow.currentImagePath
                        visible: false
                        width: 0
                        height: 0
                        cache: true
                        asynchronous: true
                        source: probePath
                        onStatusChanged: {
                            if (status === Image.Ready)
                                gallerySlideshow.cacheAspect(probePath, implicitWidth, implicitHeight)
                        }
                    }

                    Image {
                        id: aspectProbeNext
                        property string probePath: gallerySlideshow.nextImagePath
                        visible: false
                        width: 0
                        height: 0
                        cache: true
                        asynchronous: true
                        source: probePath
                        onStatusChanged: {
                            if (status === Image.Ready)
                                gallerySlideshow.cacheAspect(probePath, implicitWidth, implicitHeight)
                        }
                    }

                    Image {
                        id: currentStaticImage
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        transformOrigin: Item.Center
                        source: {
                            if (gallerySlideshow.modImages.length === 0) return ""
                            if (gallerySlideshow.currentIndex >= gallerySlideshow.modImages.length) return ""
                            var img = gallerySlideshow.modImages[gallerySlideshow.currentIndex]
                            return img && !img.isGif ? img.path : ""
                        }
                        visible: source !== ""
                        opacity: gallerySlideshow.currentOpacity
                    }

                    AnimatedImage {
                        id: currentGifImage
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        transformOrigin: Item.Center
                        source: {
                            if (gallerySlideshow.modImages.length === 0) return ""
                            if (gallerySlideshow.currentIndex >= gallerySlideshow.modImages.length) return ""
                            var img = gallerySlideshow.modImages[gallerySlideshow.currentIndex]
                            return img && img.isGif ? img.path : ""
                        }
                        visible: source !== ""
                        playing: visible
                        opacity: gallerySlideshow.currentOpacity
                    }

                    Image {
                        id: transitionStaticImage
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        transformOrigin: Item.Center
                        visible: source !== ""
                        opacity: 0
                    }

                    AnimatedImage {
                        id: transitionGifImage
                        anchors.fill: parent
                        fillMode: Image.PreserveAspectCrop
                        horizontalAlignment: Image.AlignHCenter
                        verticalAlignment: Image.AlignVCenter
                        transformOrigin: Item.Center
                        visible: source !== ""
                        playing: visible
                        opacity: 0
                    }

                    ParallelAnimation {
                        id: transitionAnim
                        onStopped: {
                            gallerySlideshow.transitioning = false

                            gallerySlideshow.scheduleNext()
                        }
                        NumberAnimation {
                            target: transitionStaticImage
                            property: "opacity"
                            to: 0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: transitionGifImage
                            property: "opacity"
                            to: 0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentStaticImage
                            property: "opacity"
                            to: gallerySlideshow.currentOpacity
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentGifImage
                            property: "opacity"
                            to: gallerySlideshow.currentOpacity
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: transitionStaticImage
                            property: "x"
                            to: -gallerySlideshow.slideOffset * gallerySlideshow.transitionDirection
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: transitionGifImage
                            property: "x"
                            to: -gallerySlideshow.slideOffset * gallerySlideshow.transitionDirection
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentStaticImage
                            property: "x"
                            to: 0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentGifImage
                            property: "x"
                            to: 0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: transitionStaticImage
                            property: "scale"
                            to: gallerySlideshow.outgoingEndScale
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: transitionGifImage
                            property: "scale"
                            to: gallerySlideshow.outgoingEndScale
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentStaticImage
                            property: "scale"
                            to: 1.0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: currentGifImage
                            property: "scale"
                            to: 1.0
                            duration: gallerySlideshow.transitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Column {
                        anchors.centerIn: parent
                        spacing: 16
                        visible: gallerySlideshow.modImages.length === 0

                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 64
                            height: 64
                            source: "qrc:/UniversalModManager/Lucide Icons/image.svg"
                            sourceSize: Qt.size(256, 256)
                            fillMode: Image.PreserveAspectFit
                            smooth: true
                            opacity: 0.6

                            layer.enabled: true
                            layer.effect: ColorOverlay {
                                color: window.textSecondaryColor
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "Set preview for your mods in the Mods Library to display here"
                            font.pixelSize: 16
                            color: window.textSecondaryColor
                            opacity: 0.5
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: parent.height * 0.25
                        radius: 20
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: "transparent" }
                            GradientStop { position: 1.0; color: "#99000000" }
                        }
                    }

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.margins: 40
                        spacing: 15
                        visible: gallerySlideshow.modImages.length > 0

                        Text {
                            text: (gallerySlideshow.modImages.length > 0 &&
                                   gallerySlideshow.currentIndex < gallerySlideshow.modImages.length) ?
                                  gallerySlideshow.modImages[gallerySlideshow.currentIndex].name : ""
                            font.pixelSize: 40
                            font.weight: 800
                            color: "#FFFFFF"
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Text {
                            text: {
                                var _ = descriptionRefreshTrigger
                                if (gallerySlideshow.modImages.length === 0) return ""
                                if (gallerySlideshow.currentIndex >= gallerySlideshow.modImages.length) return ""
                                var name = gallerySlideshow.modImages[gallerySlideshow.currentIndex].name
                                var desc = modManager.getModDescription(name)
                                return desc && desc !== "" ? desc : ""
                            }
                            visible: {
                                var _ = descriptionRefreshTrigger
                                if (gallerySlideshow.modImages.length === 0) return false
                                if (gallerySlideshow.currentIndex >= gallerySlideshow.modImages.length) return false
                                var name = gallerySlideshow.modImages[gallerySlideshow.currentIndex].name
                                var desc = modManager.getModDescription(name)
                                return desc && desc !== ""
                            }
                            font.pixelSize: 16
                            color: "#FFFFFF"
                            opacity: 0.8
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                            Layout.maximumWidth: parent.width * 0.8
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.topMargin: 10
                            spacing: 15

                            Row {
                                spacing: 10

                                Item {
                                    width: 40
                                    height: 40

                                    Item {
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        Image {
                                            id: prevIcon
                                            anchors.centerIn: parent
                                            width: 20
                                            height: 20
                                            source: "qrc:/UniversalModManager/Lucide Icons/chevron-left.svg"
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            mipmap: true
                                            cache: false
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: prevIcon
                                            source: prevIcon
                                            color: prevHover.containsMouse ? window.primaryColor : "#FFFFFF"

                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        layer.enabled: prevHover.containsMouse
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: window.primaryColor
                                            shadowBlur: 0.6
                                            shadowOpacity: 0.8
                                        }
                                    }

                                    MouseArea {
                                        id: prevHover
                                        anchors.fill: parent
                                        enabled: !gallerySlideshow.transitioning
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: gallerySlideshow.previousImage()
                                    }
                                }

                                Item {
                                    width: 40
                                    height: 40

                                    Item {
                                        anchors.centerIn: parent
                                        width: 18
                                        height: 18
                                        Image {
                                            id: playPauseIcon
                                            anchors.centerIn: parent
                                            width: 18
                                            height: 18
                                            source: gallerySlideshow.autoPlay
                                                    ? "qrc:/UniversalModManager/Lucide Icons/pause.svg"
                                                    : "qrc:/UniversalModManager/Lucide Icons/play.svg"
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            mipmap: true
                                            cache: false
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: playPauseIcon
                                            source: playPauseIcon
                                            color: playHover.containsMouse ? window.primaryColor : "#FFFFFF"

                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        layer.enabled: playHover.containsMouse
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: window.primaryColor
                                            shadowBlur: 0.6
                                            shadowOpacity: 0.8
                                        }
                                    }

                                    MouseArea {
                                        id: playHover
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        gallerySlideshow.autoPlay = !gallerySlideshow.autoPlay
                                        if (gallerySlideshow.autoPlay) {
                                            gallerySlideshow.scheduleNext()
                                        } else {
                                            gallerySlideshow.cancelNext()
                                        }
                                    }
                                    }
                                }

                                Item {
                                    width: 40
                                    height: 40

                                    Item {
                                        anchors.centerIn: parent
                                        width: 20
                                        height: 20
                                        Image {
                                            id: nextIcon
                                            anchors.centerIn: parent
                                            width: 20
                                            height: 20
                                            source: "qrc:/UniversalModManager/Lucide Icons/chevron-right.svg"
                                            fillMode: Image.PreserveAspectFit
                                            smooth: true
                                            mipmap: true
                                            cache: false
                                            visible: false
                                        }
                                        ColorOverlay {
                                            anchors.fill: nextIcon
                                            source: nextIcon
                                            color: nextHover.containsMouse ? window.primaryColor : "#FFFFFF"

                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        layer.enabled: nextHover.containsMouse
                                        layer.effect: MultiEffect {
                                            shadowEnabled: true
                                            shadowColor: window.primaryColor
                                            shadowBlur: 0.6
                                            shadowOpacity: 0.8
                                        }
                                    }

                                    MouseArea {
                                        id: nextHover
                                        anchors.fill: parent
                                        enabled: !gallerySlideshow.transitioning
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: gallerySlideshow.nextImage()
                                    }
                                }
                            }

                            Item { Layout.fillWidth: true }

                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: false
                Layout.preferredWidth: 350
                Layout.fillHeight: true
                Layout.alignment: Qt.AlignTop
                spacing: 20

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    Layout.maximumHeight: 180
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
                    radius: window.cardRadius
                    border.color: window.borderColor
                    border.width: window.borderWidth

                    gradient: modManager.theme === "Crimson" ? readyToPlayGradient : null

                    Gradient {
                        id: readyToPlayGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    layer.enabled: modManager.theme !== "Glassmorphic"
                    layer.effect: MultiEffect {
                        shadowEnabled: modManager.theme !== "Glassmorphic"
                        shadowOpacity: 0.03
                        shadowBlur: 0.1
                        shadowVerticalOffset: 4
                        shadowHorizontalOffset: 0
                        shadowColor: "#000000"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 25
                        spacing: 15

                        Text {
                            text: "Ready to Play?"
                            font.pixelSize: 18
                            font.weight: 700
                            color: window.textColor
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: "Launch Game"
                            iconSource: "qrc:/UniversalModManager/Lucide Icons/rocket.svg"
                            isPrimary: true
                            onClicked: {
                                var msg = modManager.launchGame()
                                if (msg && msg !== "") {
                                    toastNotification.show(msg)
                                }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220
                    Layout.maximumHeight: 220
                    color: modManager.theme === "Crimson" ? "transparent" : (modManager.theme === "Glassmorphic" ? window.surfaceColor : window.surfaceColor)
                    radius: window.cardRadius
                    border.color: window.borderColor
                    border.width: window.borderWidth

                    gradient: modManager.theme === "Crimson" ? quickActionsGradient : null

                    Gradient {
                        id: quickActionsGradient
                        GradientStop { position: 0.0; color: window.crimsonGradientTop }
                        GradientStop { position: 1.0; color: window.crimsonGradientBottom }
                    }

                    layer.enabled: modManager.theme !== "Glassmorphic"
                    layer.effect: MultiEffect {
                        shadowEnabled: modManager.theme !== "Glassmorphic"
                        shadowOpacity: 0.03
                        shadowBlur: 0.1
                        shadowVerticalOffset: 4
                        shadowHorizontalOffset: 0
                        shadowColor: "#000000"
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 25
                        anchors.bottomMargin: 30
                        spacing: 15

                        Text {
                            text: "Quick Actions"
                            font.pixelSize: 16
                            font.weight: 600
                            color: window.textSecondaryColor
                            Layout.fillWidth: true
                            horizontalAlignment: Text.AlignHCenter
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: "Install Local Mod"
                            icon: ""
                            isPrimary: false
                            onClicked: {
                                pageStack.currentIndex = 1
                                var modLibPage = pageStack.children[1]
                                if (modLibPage && modLibPage.openInstallDialog) {
                                    modLibPage.openInstallDialog()
                                }
                            }
                        }

                        ActionButton {
                            Layout.fillWidth: true
                            text: "Quick Conflict Scan"
                            icon: "qrc:/UniversalModManager/Lucide Icons/scan-search.svg"
                            isPrimary: false
                            onClicked: quickConflictScanDialog.open()
                        }
                    }
                }

                Item {
                    Layout.fillHeight: true
                }
            }
        }
    }

    Connections {
        target: modManager
        function onModDescriptionChanged(modName) {
            descriptionRefreshTrigger++
        }
    }

    component StatCard: Rectangle {
        property alias title: titleText.text
        property alias value: valueText.text
        property url iconSource
        property color iconBgColor: Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.18)
        property color iconColor: modManager.theme === "Glassmorphic" ? window.accentColor : window.primaryColor
        property color backgroundColor: window.surfaceColor
        property color titleColor: window.textSecondaryColor
        property color valueColor: window.textColor
        property bool isHorizontal: false

        color: modManager.theme === "Crimson" ? "transparent" :
               (modManager.theme === "Glassmorphic"
            ? window.surfaceColor
            : (modManager.theme === "Wavey"
                ? homePage.waveyBadgeColor
                : homePage.solidTint(backgroundColor)))
        Behavior on color { ColorAnimation { duration: 300 } }
        radius: window.cardRadius
        Behavior on radius { NumberAnimation { duration: 300 } }
        border.color: modManager.theme === "Glassmorphic" ? window.borderColor : "transparent"
        Behavior on border.color { ColorAnimation { duration: 300 } }
        border.width: modManager.theme === "Glassmorphic" ? 1 : 0
        Behavior on border.width { NumberAnimation { duration: 300 } }

        gradient: modManager.theme === "Crimson" ? statCardGradient : null

        Gradient {
            id: statCardGradient
            GradientStop { position: 0.0; color: window.crimsonGradientTop }
            GradientStop { position: 1.0; color: window.crimsonGradientBottom }
        }

        layer.enabled: modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: modManager.theme !== "Glassmorphic"
            shadowOpacity: 0.03
            shadowBlur: 0.1
            shadowVerticalOffset: 4
            shadowHorizontalOffset: 0
            shadowColor: "#000000"
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 20

            Item {
                Layout.preferredWidth: 40
                Layout.preferredHeight: 40
                Layout.alignment: Qt.AlignCenter

                Image {
                    id: iconImage
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 2
                    width: 40
                    height: 40
                    source: iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    cache: false
                }

                ColorOverlay {
                    anchors.fill: iconImage
                    source: iconImage
                    color: iconColor
                    visible: modManager.theme === "Glassmorphic"
                }
            }

            Column {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    id: valueText
                    font.pixelSize: 24
                    font.weight: 800
                    color: valueColor
                    Behavior on color { ColorAnimation { duration: 300 } }
                }

                Text {
                    id: titleText
                    font.pixelSize: 14
                    font.weight: 600
                    color: titleColor
                    Behavior on color { ColorAnimation { duration: 300 } }
                }
            }
        }
    }

    component ActionButton: Rectangle {
        property alias text: buttonText.text
        property alias icon: iconText.text
        property url iconSource: ""
        property int iconSize: 18
        property bool isPrimary: false
        signal clicked()
        property bool hovered: false

        height: 50
        color: isPrimary ? (hovered ? Qt.darker(window.primaryColor, 1.1) : window.primaryColor) : window.surfaceColor
        border.color: isPrimary ? "transparent" : (hovered ? window.accentColor : window.borderColor)
        border.width: window.borderWidth
        radius: 14

        layer.enabled: isPrimary && modManager.theme !== "Glassmorphic"
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowOpacity: 0.3
            shadowBlur: 0.4
            shadowVerticalOffset: 4
            shadowHorizontalOffset: 0
            shadowColor: window.primaryColor
        }

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        RowLayout {
            anchors.centerIn: parent
            spacing: (iconSource !== "" || iconText.text !== "") ? 10 : 0

            Item {
                id: iconContainer
                visible: iconSource !== "" || iconText.text !== ""
                width: iconSource !== "" ? iconSize : 0
                height: iconSource !== "" ? iconSize : 0

                Image {
                    id: iconImage
                    anchors.centerIn: parent
                    width: iconSize
                    height: iconSize
                    source: iconSource
                    fillMode: Image.PreserveAspectFit
                    smooth: true
                    mipmap: true
                    cache: false
                    visible: iconSource !== ""
                }
                ColorOverlay {
                    anchors.fill: iconImage
                    source: iconImage
                    color: isPrimary
                        ? "#FFFFFF"
                        : (modManager.theme === "Glassmorphic"
                           ? window.accentColor
                           : (hovered ? window.accentColor : window.textColor))
                    visible: iconSource !== ""
                }

                Text {
                    id: iconText
                    anchors.centerIn: parent
                    visible: iconSource === "" && iconText.text !== ""
                    font.pixelSize: 16
                    color: isPrimary
                        ? "#FFFFFF"
                        : (modManager.theme === "Glassmorphic"
                           ? window.accentColor
                           : (hovered ? window.accentColor : window.textColor))
                    Behavior on color { ColorAnimation { duration: 150 } }
                }
            }

            Text {
                id: buttonText
                font.pixelSize: 15
                font.weight: 700
                color: isPrimary ? "#FFFFFF" : (hovered ? window.accentColor : window.textColor)
                Behavior on color { ColorAnimation { duration: 150 } }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onEntered: parent.hovered = true
            onExited: parent.hovered = false
            onClicked: parent.clicked()
        }
    }

    HomePageNotifications {
        id: homePageNotifications
        anchors.fill: parent
    }

    readonly property alias toastNotification: homePageNotifications.toastNotification

    QuickConflictScanDialog {
        id: quickConflictScanDialog
    }
}
