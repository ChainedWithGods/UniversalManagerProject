import QtQuick

Item {
    id: root
    visible: false

    property var toastNotification: root

    function show(message, duration) {
        notificationManager.notify("", message, "",
                                   duration !== undefined ? duration : 2800)
    }
}
