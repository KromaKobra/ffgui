pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// A tiny speaker diagram: one dot for mono, two for stereo, a ring for more.
Item {
    id: icon

    property string layout: "stereo"
    property int channels: 2
    readonly property int count: icon.layout === "mono" ? 1 : Math.max(1, Math.min(8, icon.channels))

    implicitWidth: 18
    implicitHeight: 12

    Repeater {
        model: icon.count
        delegate: Rectangle {
            required property int index
            width: 5
            height: 5
            radius: 2.5
            color: Theme.textMuted
            x: icon.count === 1 ? 6.5 : icon.count === 2 ? 2 + index * 9
               : 6.5 + 6 * Math.cos(index / icon.count * 2 * Math.PI)
            y: icon.count <= 2 ? 3.5 : 3.5 + 4 * Math.sin(index / icon.count * 2 * Math.PI)
        }
    }
}
