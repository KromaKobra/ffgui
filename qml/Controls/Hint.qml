import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FFgui.Theme

// A dim, wrapping sentence that explains something.
Label {
    Layout.fillWidth: true
    font.pixelSize: Metrics.fontCaption
    opacity: 0.5
    wrapMode: Text.Wrap
}
