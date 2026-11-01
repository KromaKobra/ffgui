import QtQuick
import QtQuick.Layouts
import FFgui.Backend
import FFgui.Controls
import FFgui.Theme

// Start / end fields for a time range, with a switch to turn the range on.
// Edits here and drags of the band in the Media View change the same values.
ColumnLayout {
    id: editor

    property bool enabledRange: false
    property real start: 0
    property real end: 0
    property bool supported: true
    property string unsupportedText: qsTr("This filter can't be limited to a time range.")

    signal toggled(bool on)
    signal edited(real start, real end)

    Layout.fillWidth: true
    spacing: Metrics.spacingSmall

    FfCheckBox {
        text: qsTr("Only during a time range")
        enabled: editor.supported
        checked: editor.enabledRange
        onToggled: editor.toggled(checked)
    }
    Hint {
        visible: !editor.supported
        text: editor.unsupportedText
    }
    RowLayout {
        visible: editor.enabledRange
        Layout.fillWidth: true
        TimeField {
            Layout.fillWidth: true
            seconds: editor.start
            display: Format.timecode(editor.start, true)
            onEdited: s => editor.edited(Math.min(s, editor.end), Math.max(s, editor.end))
        }
        Caption { text: Glyphs.arrow }
        TimeField {
            Layout.fillWidth: true
            seconds: editor.end
            display: Format.timecode(editor.end, true)
            onEdited: s => editor.edited(Math.min(editor.start, s), Math.max(editor.start, s))
        }
    }
}
