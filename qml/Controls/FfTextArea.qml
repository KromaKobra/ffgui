import QtQuick
import QtQuick.Controls
import FFgui.Theme

TextArea {
    id: control

    property bool mono: false

    font.pixelSize: Metrics.fontBody
    selectByMouse: true
    wrapMode: TextEdit.Wrap
    Binding on font.family {
        when: control.mono
        value: Metrics.monoFamily
    }
}
