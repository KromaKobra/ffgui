import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// A label and the control(s) that set it, on one line.
RowLayout {
    id: row

    property string label
    property real labelWidth: Metrics.formLabelWidth

    Layout.fillWidth: true
    spacing: Metrics.spacingLarge

    Label {
        text: row.label
        opacity: 0.6
        font.pixelSize: Metrics.fontCaption
        elide: Text.ElideRight
        Layout.preferredWidth: row.labelWidth
    }
}
