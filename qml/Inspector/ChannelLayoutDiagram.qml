pragma ComponentBehavior: Bound
import QtQuick
import FFgui.Theme

// The speakers of a channel layout, labelled FL, FR, FC, LFE, ...
Flow {
    id: diagram

    property var names: []

    spacing: Metrics.spacingLarge

    Repeater {
        model: diagram.names
        delegate: Rectangle {
            id: speaker
            required property string modelData
            width: 42
            height: 32
            radius: Metrics.radiusMedium
            color: Theme.control
            border.color: Theme.borderStrong
            Text { anchors.centerIn: parent; text: speaker.modelData; color: Theme.text; font.bold: true }
        }
    }
}
