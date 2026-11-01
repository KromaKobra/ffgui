import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// ffmpeg command or filter text, in the command colour.
Label {
    Layout.fillWidth: true
    font.family: Metrics.monoFamily
    font.pixelSize: Metrics.fontSmall
    color: Theme.command
    wrapMode: Text.WrapAnywhere
}
