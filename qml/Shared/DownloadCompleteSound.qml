import QtQuick
import QtMultimedia

Item {
    id: root
    visible: false
    width: 0
    height: 0

    SoundEffect {
        id: completeSound
        source: "qrc:/UniversalModManager/assets/Notifications/downloaded.wav"
        volume: 0.5
    }

    Connections {
        target: downloadManager
        function onDownloadCompleted(filePath) { completeSound.play() }
    }
}
