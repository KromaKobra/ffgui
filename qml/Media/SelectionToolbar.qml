pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// The floating mini-bar over a selected time range.
Rectangle {
    id: bar

    signal action(string id)

    readonly property var actions: [
        { id: "trim", label: qsTr("Trim to selection") },
        { id: "cut", label: qsTr("Cut out") },
        { id: "fadeIn", label: qsTr("Fade in") },
        { id: "fadeOut", label: qsTr("Fade out") },
        { id: "silence", label: qsTr("Silence") },
        { id: "normalize", label: qsTr("Normalize") },
        { id: "step", label: qsTr("Apply step…") },
        { id: "loop", label: qsTr("Loop") },
        { id: "export", label: qsTr("Export…") },
        { id: "marker", label: qsTr("Add marker") }
    ]

    width: row.implicitWidth + 8
    height: 30
    radius: Metrics.radiusLarge
    color: Theme.card
    border.color: Theme.borderStrong

    Row {
        id: row
        anchors.centerIn: parent
        Repeater {
            model: bar.actions
            delegate: FfToolButton {
                required property var modelData
                text: modelData.label
                font.pixelSize: Metrics.fontCaption
                implicitHeight: 26
                checked: modelData.id === "loop" && App.transport.loop
                onClicked: bar.action(modelData.id)
            }
        }
    }
}
