pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Brief messages (render done, analysis done, errors) with an optional
// action. They stack in the corner and leave on their own.
Item {
    id: host

    property int timeout: 4500

    ListModel { id: toasts }

    Connections {
        target: App
        function onToastRequested(text, kind, actionText, actionId) {
            // The same message twice in a row just restarts its timer.
            if (toasts.count > 0 && toasts.get(toasts.count - 1).text === text)
                toasts.remove(toasts.count - 1);
            toasts.append({ text: text, kind: kind, actionText: actionText, actionId: actionId });
            while (toasts.count > 4)
                toasts.remove(0);
        }
    }

    ColumnLayout {
        anchors { right: parent.right; bottom: parent.bottom; margins: Metrics.spacingWide }
        spacing: Metrics.spacing

        Repeater {
            model: toasts
            delegate: Rectangle {
                id: toast
                required property int index
                required property string text
                required property string kind
                required property string actionText
                required property string actionId
                Layout.alignment: Qt.AlignRight
                implicitWidth: Math.min(420, row.implicitWidth + 24)
                implicitHeight: 36
                radius: Metrics.radiusLarge
                color: Theme.card
                border.color: toast.kind === "error" ? Theme.error : toast.kind === "warning" ? Theme.warn
                              : toast.kind === "success" ? Theme.ok : Theme.borderStrong

                Timer {
                    interval: toast.kind === "error" ? host.timeout * 2 : host.timeout
                    running: !hover.hovered
                    onTriggered: toasts.remove(toast.index)
                }
                HoverHandler { id: hover }

                RowLayout {
                    id: row
                    anchors { fill: parent; leftMargin: 12; rightMargin: 6 }
                    spacing: Metrics.spacingLarge
                    Text {
                        text: toast.text
                        color: Theme.text
                        font.pixelSize: Metrics.fontBody
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.maximumWidth: 320
                    }
                    FfToolButton {
                        visible: toast.actionText !== ""
                        text: toast.actionText
                        font.pixelSize: Metrics.fontCaption
                        onClicked: {
                            App.runAction(toast.actionId);
                            toasts.remove(toast.index);
                        }
                    }
                    FfToolButton {
                        text: Glyphs.close
                        font.pixelSize: Metrics.fontSmall
                        onClicked: toasts.remove(toast.index)
                    }
                }
            }
        }
    }
}
