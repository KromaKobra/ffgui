import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// A labelled time entry: "Start  [00:01.250]".
RowLayout {
    id: row

    property string label
    property real seconds: 0
    property string display

    signal edited(real seconds)

    Layout.fillWidth: true
    spacing: Metrics.spacingLarge

    Label {
        text: row.label
        opacity: 0.6
        font.pixelSize: Metrics.fontCaption
        Layout.preferredWidth: 60
    }
    TimeField {
        Layout.fillWidth: true
        seconds: row.seconds
        display: row.display
        onEdited: s => row.edited(s)
    }
}
