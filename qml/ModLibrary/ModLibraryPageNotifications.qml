import QtQuick

Item {
    id: root
    visible: false

    property var toastNotification: root

    function show(message, option) {
        if (typeof option === "number")
            notificationManager.notify("", message, "", option)
        else
            notificationManager.notify("", message, option || "", 2800)
    }
}
