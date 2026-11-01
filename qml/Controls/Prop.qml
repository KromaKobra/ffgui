import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// One "key  value" line of a property sheet.
RowLayout {
    id: prop

    property string key
    property string value
    property bool mono: false
    property color valueColor: Theme.text

    Layout.fillWidth: true
    spacing: Metrics.spacingLarge

    Label {
        text: prop.key
        opacity: 0.6
        font.pixelSize: Metrics.fontCaption
        Layout.preferredWidth: Metrics.propKeyWidth
        Layout.alignment: Qt.AlignTop
        elide: Text.ElideRight
    }
    Label {
        text: prop.value
        color: prop.valueColor
        font.pixelSize: Metrics.fontCaption
        Binding on font.family {
            when: prop.mono
            value: Metrics.monoFamily
        }
        wrapMode: Text.Wrap
        Layout.fillWidth: true
    }
}
