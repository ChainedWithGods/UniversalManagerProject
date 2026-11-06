import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import Qt5Compat.GraphicalEffects

ComboBox {
    property color panelColor: window.surfaceColor
    id: filterCombo
    Layout.fillWidth: true
    Layout.preferredHeight: 34
    textRole: "label"

    contentItem: Text {
        leftPadding: 10
        rightPadding: 24
        text: filterCombo.displayText
        font.pixelSize: 12
        color: window.textColor
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Image {
        x: filterCombo.width - width - 10
        y: (filterCombo.height - height) / 2
        width: 14
        height: 14
        source: "qrc:/UniversalModManager/Lucide Icons/chevron-down.svg"
        sourceSize: Qt.size(14, 14)
        smooth: true

        layer.enabled: true
        layer.effect: ColorOverlay {
            color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, filterCombo.popup.visible ? 0.9 : 0.55)
        }
    }

    background: Rectangle {
        radius: 6
        color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, filterCombo.pressed ? 0.09 : 0.04)
        border.width: 1
        border.color: filterCombo.activeFocus ? window.primaryColor : window.borderColor
        Behavior on border.color { ColorAnimation { duration: 150 } }
    }

    popup: Popup {
        y: filterCombo.height + 4
        width: filterCombo.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 340)
        padding: 4

        background: Rectangle {
            color: panelColor
            radius: 8
            border.width: 1
            border.color: window.borderColor

            layer.enabled: true
            layer.effect: MultiEffect {
                shadowEnabled: true
                shadowColor: Qt.rgba(0, 0, 0, 0.15)
                shadowVerticalOffset: 4
                shadowBlur: 0.8
            }
        }

        contentItem: ListView {
            id: tagList
            clip: true
            implicitHeight: contentHeight
            model: filterCombo.popup.visible ? filterCombo.delegateModel : null
            currentIndex: filterCombo.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds

            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => {
                    const maxY = Math.max(0, tagList.contentHeight - tagList.height)
                    const targetY = Math.max(0, Math.min(maxY, tagList.contentY - event.angleDelta.y * 1.2))
                    tagScrollAnim.stop()
                    tagScrollAnim.to = targetY
                    tagScrollAnim.start()
                    event.accepted = true
                }
            }

            NumberAnimation {
                id: tagScrollAnim
                target: tagList
                property: "contentY"
                duration: 150
                easing.type: Easing.OutCubic
            }

            ScrollBar.vertical: ScrollBar {
                id: tagScrollBar
                policy: ScrollBar.AsNeeded
                width: 10
                padding: 3

                contentItem: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: tagScrollBar.pressed
                        ? window.primaryColor
                        : (tagScrollBar.hovered
                            ? Qt.rgba(window.primaryColor.r, window.primaryColor.g, window.primaryColor.b, 0.7)
                            : Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.3))

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                background: Rectangle {
                    implicitWidth: 4
                    radius: 2
                    color: Qt.rgba(window.textColor.r, window.textColor.g, window.textColor.b, 0.07)
                }
            }
        }
    }

    delegate: ItemDelegate {
        id: tagDelegate
        width: filterCombo.width - 12
        height: 30
        padding: 0

        contentItem: Text {
            text: (typeof modelData === "object" && modelData !== null) ? modelData.label : modelData
            color: (tagDelegate.hovered || tagDelegate.highlighted) ? "#FFFFFF" : window.textColor
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            leftPadding: 10
            rightPadding: 14
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: (tagDelegate.hovered || tagDelegate.highlighted) ? window.primaryColor : "transparent"
            radius: 6

            Behavior on color { ColorAnimation { duration: 100 } }
        }
    }
}
